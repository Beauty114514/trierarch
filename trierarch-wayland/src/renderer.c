/*
 * EGL/GLES2: SHM texture + EGL Wayland buffer import for KDE/llvmpipe.
 *
 * Texture shaders follow wlroots GLES2 (see swaywm/wlroots render/gles2/shaders.c):
 * tex_rgba vs tex_rgbx — XRGB wl_shm must ignore the unused alpha channel (vec4(rgb, 1.0)).
 * Blend: glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA) as in wlroots gles2_begin().
 *
 * dmabuf rendering:
 *   1. Try zero-copy EGL import via eglCreateImageKHR(EGL_LINUX_DMA_BUF_EXT).
 *   2. If the host EGL cannot import the guest’s dmabuf, fall back to mmap +
 *      glTexImage2D (same cost as wl_shm; still useful when import fails).
 */
#include "renderer.h"
#include "server_internal.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <android/log.h>

#ifndef DRM_FORMAT_XRGB8888
#define DRM_FORMAT_XRGB8888 ((uint32_t)0x34325258)
#endif
#ifndef DRM_FORMAT_ARGB8888
#define DRM_FORMAT_ARGB8888 ((uint32_t)0x34325241)
#endif
#ifndef DRM_FORMAT_XBGR8888
#define DRM_FORMAT_XBGR8888 ((uint32_t)0x34324258)
#endif
#ifndef DRM_FORMAT_ABGR8888
#define DRM_FORMAT_ABGR8888 ((uint32_t)0x34324241)
#endif

#ifndef EGL_LINUX_DMA_BUF_EXT
#define EGL_LINUX_DMA_BUF_EXT 0x3270
#endif
#ifndef EGL_LINUX_DRM_FOURCC_EXT
#define EGL_LINUX_DRM_FOURCC_EXT 0x3271
#endif
#ifndef EGL_DMA_BUF_PLANE0_FD_EXT
#define EGL_DMA_BUF_PLANE0_FD_EXT 0x3272
#endif
#ifndef EGL_DMA_BUF_PLANE0_OFFSET_EXT
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT 0x3273
#endif
#ifndef EGL_DMA_BUF_PLANE0_STRIDE_EXT
#define EGL_DMA_BUF_PLANE0_STRIDE_EXT 0x3274
#endif
#ifndef EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT 0x3284
#endif
#ifndef EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT 0x3285
#endif

/* linux/dma-buf.h — not always in NDK sysroot; define the bare minimum. */
#ifndef DMA_BUF_IOCTL_SYNC
struct trierarch_dma_buf_sync { uint64_t flags; };
#define DMA_BUF_SYNC_READ      (1 << 0)
#define DMA_BUF_SYNC_START     (0 << 2)
#define DMA_BUF_SYNC_END       (1 << 2)
#define DMA_BUF_IOCTL_SYNC     _IOW('b', 0, struct trierarch_dma_buf_sync)
#endif

#define LOG_TAG "TrierarchRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* wl_shm / XRGB ARGB dmabuf mmap: memory is typically BGRA order → shader swizzle 1.
 * ABGR/XBGR mmap: byte order matches GL_RGBA upload → no BGR swap. */
static float dmabuf_mmap_swizzle_for_drm_fmt(uint32_t drm_fmt) {
    switch (drm_fmt) {
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XBGR8888:
        return 0.0f;
    default:
        return 1.0f;
    }
}

#ifndef EGL_WAYLAND_BUFFER_WL
#define EGL_WAYLAND_BUFFER_WL 0x31D5
#endif

typedef EGLBoolean (*PFN_eglBindWaylandDisplayWL)(EGLDisplay, void *);
typedef EGLBoolean (*PFN_eglQueryWaylandBufferWL)(EGLDisplay, void *, EGLint, EGLint *);
typedef void *(*PFN_eglCreateImageKHR)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint *);
typedef EGLBoolean (*PFN_eglDestroyImageKHR)(EGLDisplay, void *);
typedef void (*PFN_glEGLImageTargetTexture2DOES)(GLenum, void *);

struct renderer_context {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;
    ANativeWindow *window;
    int width, height;
    bool valid;
    GLuint tex_prog_rgba, tex_prog_rgbx, bg_prog, tex_id;
    PFN_eglQueryWaylandBufferWL eglQueryWaylandBufferWL;
    PFN_eglCreateImageKHR eglCreateImageKHR;
    PFN_eglDestroyImageKHR eglDestroyImageKHR;
    PFN_glEGLImageTargetTexture2DOES glEGLImageTargetTexture2DOES;
};

static const char *vert_src = "attribute vec4 a_position; void main() { gl_Position = a_position; }\n";
static const char *vert_tex_src = "attribute vec4 a_position; attribute vec2 a_texcoord; varying vec2 v_texcoord; void main() { gl_Position = a_position; v_texcoord = a_texcoord; }\n";
static const char *frag_bg_src = "precision mediump float; void main() { gl_FragColor = vec4(0.0,0.0,0.0,1.0); }\n";
/* u_swizzle=1: SHM BGRA memory as GL_RGBA sample fix (same idea as upload swizzle elsewhere). */
static const char *frag_tex_rgba_src =
    "precision mediump float; varying vec2 v_texcoord; uniform sampler2D u_tex; uniform float u_swizzle; "
    "void main() { vec4 c = texture2D(u_tex, v_texcoord); vec4 bgra = vec4(c.b, c.g, c.r, c.a); gl_FragColor = mix(c, bgra, u_swizzle); }\n";
/* wlroots tex_fragment_src_rgbx: vec4(texture2D(...).rgb, 1.0) — wl_shm XRGB8888 */
static const char *frag_tex_rgbx_src =
    "precision mediump float; varying vec2 v_texcoord; uniform sampler2D u_tex; uniform float u_swizzle; "
    "void main() { vec4 c = texture2D(u_tex, v_texcoord); vec4 s = mix(c, vec4(c.b, c.g, c.r, c.a), u_swizzle); "
    "gl_FragColor = vec4(s.rgb, 1.0); }\n";

static GLuint make_program(const char *v, const char *f) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &v, NULL);
    glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &f, NULL);
    glCompileShader(fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

renderer_context_t *renderer_create(ANativeWindow *window, struct wayland_server *srv, int skip_egl_wl_bind) {
    if (!window) return NULL;
    renderer_context_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->window = window;
    ctx->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (ctx->display == EGL_NO_DISPLAY) { free(ctx); return NULL; }
    EGLint major, minor;
    if (!eglInitialize(ctx->display, &major, &minor)) { eglTerminate(ctx->display); free(ctx); return NULL; }
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config = NULL;
    EGLint num = 0;
    if (!eglChooseConfig(ctx->display, config_attribs, &config, 1, &num) || num == 0) { eglTerminate(ctx->display); free(ctx); return NULL; }
    ctx->config = config;
    const EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    ctx->context = eglCreateContext(ctx->display, config, EGL_NO_CONTEXT, ctx_attribs);
    if (ctx->context == EGL_NO_CONTEXT) { eglTerminate(ctx->display); free(ctx); return NULL; }
    ctx->surface = eglCreateWindowSurface(ctx->display, config, window, NULL);
    if (ctx->surface == EGL_NO_SURFACE) { eglDestroyContext(ctx->display, ctx->context); eglTerminate(ctx->display); free(ctx); return NULL; }
    if (!eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context)) { eglDestroySurface(ctx->display, ctx->surface); eglDestroyContext(ctx->display, ctx->context); eglTerminate(ctx->display); free(ctx); return NULL; }
    ctx->eglCreateImageKHR = (PFN_eglCreateImageKHR)eglGetProcAddress("eglCreateImageKHR");
    ctx->eglDestroyImageKHR = (PFN_eglDestroyImageKHR)eglGetProcAddress("eglDestroyImageKHR");
    ctx->glEGLImageTargetTexture2DOES = (PFN_glEGLImageTargetTexture2DOES)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    ctx->eglQueryWaylandBufferWL = (PFN_eglQueryWaylandBufferWL)eglGetProcAddress("eglQueryWaylandBufferWL");
    if (!skip_egl_wl_bind && srv && ctx->eglQueryWaylandBufferWL && ctx->eglCreateImageKHR && ctx->glEGLImageTargetTexture2DOES) {
        PFN_eglBindWaylandDisplayWL bind_wl = (PFN_eglBindWaylandDisplayWL)eglGetProcAddress("eglBindWaylandDisplayWL");
        void *wl_display = compositor_get_wl_display((wayland_server_t *)srv);
        if (bind_wl && wl_display && bind_wl(ctx->display, wl_display)) {
            compositor_set_egl_buffer_supported((wayland_server_t *)srv, true);
            LOGI("eglBindWaylandDisplayWL OK, EGL buffer import enabled");
        }
    } else if (skip_egl_wl_bind) {
        LOGI("skip eglBindWaylandDisplayWL: guest buffers must use linux-dmabuf (mmap fallback if EGL import fails)");
    }
    eglSwapInterval(ctx->display, 1);
    eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglQuerySurface(ctx->display, ctx->surface, EGL_WIDTH, &ctx->width);
    eglQuerySurface(ctx->display, ctx->surface, EGL_HEIGHT, &ctx->height);
    ctx->valid = true;
    return ctx;
}

void renderer_destroy(renderer_context_t *ctx) {
    if (!ctx) return;
    if (ctx->valid) {
        eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(ctx->display, ctx->surface);
        eglDestroyContext(ctx->display, ctx->context);
    }
    eglTerminate(ctx->display);
    ctx->valid = false;
    free(ctx);
}

void renderer_release_context(renderer_context_t *ctx) {
    if (!ctx || !ctx->valid) return;
    if (ctx->tex_id) { glDeleteTextures(1, &ctx->tex_id); ctx->tex_id = 0; }
    if (ctx->tex_prog_rgba) { glDeleteProgram(ctx->tex_prog_rgba); ctx->tex_prog_rgba = 0; }
    if (ctx->tex_prog_rgbx) { glDeleteProgram(ctx->tex_prog_rgbx); ctx->tex_prog_rgbx = 0; }
    if (ctx->bg_prog) { glDeleteProgram(ctx->bg_prog); ctx->bg_prog = 0; }
    eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool renderer_is_valid(renderer_context_t *ctx) { return ctx && ctx->valid; }
void renderer_get_size(renderer_context_t *ctx, int *w, int *h) { if (ctx && w) *w = ctx->width; if (ctx && h) *h = ctx->height; }

typedef struct { renderer_context_t *ctx; int32_t out_w; int32_t out_h; } draw_user_t;

static int draw_surface_cb(const compositor_surface_view_t *view, void *user) {
    draw_user_t *ru = user;
    renderer_context_t *ctx = ru->ctx;
    if (!view || (view->pixels == NULL && view->egl_buffer == NULL && view->dmabuf == NULL)
            || view->width <= 0 || view->height <= 0)
        return 0;
    if (!ctx->tex_prog_rgba) {
        ctx->tex_prog_rgba = make_program(vert_tex_src, frag_tex_rgba_src);
        ctx->tex_prog_rgbx = make_program(vert_tex_src, frag_tex_rgbx_src);
        glGenTextures(1, &ctx->tex_id);
        if (!ctx->tex_prog_rgba || !ctx->tex_prog_rgbx || !ctx->tex_id) return 1;
    }

    int buf_w = view->buf_width > 0 ? view->buf_width : view->width;
    int buf_h = view->buf_height > 0 ? view->buf_height : view->height;
    void *egl_image_to_destroy = NULL;
    float swizzle = 1.0f;
    bool tex_uploaded = false;

    if (view->dmabuf && ctx->eglCreateImageKHR && ctx->eglDestroyImageKHR && ctx->glEGLImageTargetTexture2DOES) {
        struct dmabuf_buffer *db = view->dmabuf;
        void *img = NULL;
        int fd = dup(db->dmabuf_fd);
        if (fd >= 0) {
            EGLint attrs[] = {
                EGL_WIDTH, db->width,
                EGL_HEIGHT, db->height,
                EGL_LINUX_DRM_FOURCC_EXT, (EGLint)db->drm_format,
                EGL_DMA_BUF_PLANE0_FD_EXT, fd,
                EGL_DMA_BUF_PLANE0_OFFSET_EXT, (EGLint)db->offset,
                EGL_DMA_BUF_PLANE0_STRIDE_EXT, (EGLint)db->stride,
                EGL_NONE
            };
            img = ctx->eglCreateImageKHR(ctx->display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
            close(fd);
            if (!img)
                (void)eglGetError();
            if (!img && db->modifier != 0ull) {
                fd = dup(db->dmabuf_fd);
                if (fd >= 0) {
                    uint32_t mod_lo = (uint32_t)(db->modifier & 0xffffffffu);
                    uint32_t mod_hi = (uint32_t)(db->modifier >> 32);
                    EGLint attrs_m[] = {
                        EGL_WIDTH, db->width,
                        EGL_HEIGHT, db->height,
                        EGL_LINUX_DRM_FOURCC_EXT, (EGLint)db->drm_format,
                        EGL_DMA_BUF_PLANE0_FD_EXT, fd,
                        EGL_DMA_BUF_PLANE0_OFFSET_EXT, (EGLint)db->offset,
                        EGL_DMA_BUF_PLANE0_STRIDE_EXT, (EGLint)db->stride,
                        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, (EGLint)mod_lo,
                        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (EGLint)mod_hi,
                        EGL_NONE
                    };
                    img = ctx->eglCreateImageKHR(ctx->display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attrs_m);
                    close(fd);
                    if (!img)
                        LOGE("eglCreateImageKHR dma-buf+modifier failed fmt=0x%x mod=0x%llx err=0x%x",
                                db->drm_format, (unsigned long long)db->modifier, eglGetError());
                }
            } else if (!img) {
                LOGE("eglCreateImageKHR dma-buf failed (fmt=0x%x err=0x%x); mmap fallback",
                        db->drm_format, eglGetError());
            }
            if (img) {
                static unsigned egl_import_ok_logged;
                if (egl_import_ok_logged < 16) {
                    LOGI("dmabuf EGL import OK #%u: %dx%d fmt=0x%x mod=0x%016llx",
                            egl_import_ok_logged + 1, db->width, db->height, db->drm_format,
                            (unsigned long long)db->modifier);
                    egl_import_ok_logged++;
                }
                glBindTexture(GL_TEXTURE_2D, ctx->tex_id);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                ctx->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, img);
                egl_image_to_destroy = img;
                swizzle = 0.0f;
                tex_uploaded = true;
            }
        }
    }

    /* Fallback: host EGL cannot import the guest dmabuf (typical on Android where
     * the host EGL rejects certain guest dmabufs).
     * Read the pixel data via mmap and upload as texture — same path as wl_shm. */
    static bool dmabuf_mmap_fallback_logged = false;
    if (view->dmabuf && egl_image_to_destroy == NULL) {
        struct dmabuf_buffer *db = view->dmabuf;
        if (db->dmabuf_fd >= 0 && db->stride > 0 && db->width > 0 && db->height > 0) {
            size_t map_len = (size_t)db->offset + (size_t)db->stride * (size_t)db->height;
            int mfd = dup(db->dmabuf_fd);
            if (mfd >= 0) {
                struct trierarch_dma_buf_sync sync;
                sync.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ;
                ioctl(mfd, DMA_BUF_IOCTL_SYNC, &sync);
                void *map = mmap(NULL, map_len, PROT_READ, MAP_SHARED, mfd, 0);
                if (map != MAP_FAILED) {
                    const unsigned char *pixels = (const unsigned char *)map + db->offset;
                    int row = db->width * 4;
                    glBindTexture(GL_TEXTURE_2D, ctx->tex_id);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    if ((int)db->stride == row) {
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, db->width, db->height, 0,
                                GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                        tex_uploaded = true;
                    } else {
                        unsigned char *tmp = malloc((size_t)db->height * row);
                        if (tmp) {
                            const unsigned char *src = pixels;
                            for (int y = 0; y < db->height; y++) {
                                memcpy(tmp + y * row, src, row);
                                src += db->stride;
                            }
                            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, db->width, db->height, 0,
                                    GL_RGBA, GL_UNSIGNED_BYTE, tmp);
                            free(tmp);
                            tex_uploaded = true;
                        }
                    }
                    swizzle = dmabuf_mmap_swizzle_for_drm_fmt(db->drm_format);
                    if (tex_uploaded && !dmabuf_mmap_fallback_logged) {
                        LOGI("dmabuf mmap fallback active: %dx%d stride=%u fmt=0x%x mod=0x%016llx (CPU readback; host EGL did not import)",
                                db->width, db->height, db->stride, db->drm_format,
                                (unsigned long long)db->modifier);
                        dmabuf_mmap_fallback_logged = true;
                    }
                    munmap(map, map_len);
                } else {
                    static unsigned mmap_fail_logged;
                    if (mmap_fail_logged < 16) {
                        LOGE("mmap dmabuf fallback failed: %dx%d fmt=0x%x mod=0x%016llx len=%zu fd=%d errno=%d",
                                db->width, db->height, db->drm_format,
                                (unsigned long long)db->modifier, map_len, mfd, errno);
                        mmap_fail_logged++;
                    }
                }
                sync.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ;
                ioctl(mfd, DMA_BUF_IOCTL_SYNC, &sync);
                close(mfd);
            }
        }
    }

    if (egl_image_to_destroy == NULL && view->egl_buffer && ctx->eglQueryWaylandBufferWL && ctx->eglCreateImageKHR && ctx->glEGLImageTargetTexture2DOES) {
        EGLint qw = 0, qh = 0;
        if (ctx->eglQueryWaylandBufferWL(ctx->display, view->egl_buffer, 0x3057, &qw)
                && ctx->eglQueryWaylandBufferWL(ctx->display, view->egl_buffer, 0x3056, &qh)) {
            buf_w = qw;
            buf_h = qh;
        }
        void *img = ctx->eglCreateImageKHR(ctx->display, ctx->context, EGL_WAYLAND_BUFFER_WL,
                (EGLClientBuffer)view->egl_buffer, NULL);
        if (img) {
            glBindTexture(GL_TEXTURE_2D, ctx->tex_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            ctx->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, img);
            egl_image_to_destroy = img;
            swizzle = 0.0f;
            tex_uploaded = true;
        } else {
            LOGE("EGL_WAYLAND_BUFFER_WL eglCreateImageKHR failed err=0x%x", eglGetError());
        }
    }

    if (view->pixels && egl_image_to_destroy == NULL) {
        int row = buf_w * 4;
        if (view->stride < row) return 0;
        glBindTexture(GL_TEXTURE_2D, ctx->tex_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        if (view->stride == row)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, buf_w, buf_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, view->pixels);
        else {
            unsigned char *buf = malloc((size_t)buf_h * row);
            if (!buf) return 0;
            const unsigned char *src = view->pixels;
            for (int y = 0; y < buf_h; y++) { memcpy(buf + y * row, src, row); src += view->stride; }
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, buf_w, buf_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
            free(buf);
        }
        tex_uploaded = true;
    }

    if (!tex_uploaded) {
        static int no_tex_warn;
        if (no_tex_warn < 32) {
            if (view->dmabuf) {
                struct dmabuf_buffer *db = view->dmabuf;
                LOGE("no texture after dmabuf paths (EGL+mmap): %dx%d logical %dx%d fmt=0x%x stride=%u mod=0x%016llx",
                        db->width, db->height, (int)view->width, (int)view->height,
                        db->drm_format, db->stride, (unsigned long long)db->modifier);
            } else {
                LOGE("no texture data uploaded (dmabuf=%p egl_buf=%p pixels=%p %dx%d)",
                        (void *)view->dmabuf, view->egl_buffer, view->pixels,
                        (int)view->width, (int)view->height);
            }
            no_tex_warn++;
        }
        return 0;
    }

    int32_t out_w = ru->out_w > 0 ? ru->out_w : ctx->width;
    int32_t out_h = ru->out_h > 0 ? ru->out_h : ctx->height;
    float x0, y0, x1, y1;
    if (view->position_in_physical) {
        /* Cursor: position and size are already in physical pixels. */
        x0 = (float)view->x;
        y0 = (float)view->y;
        x1 = (float)(view->x + view->width);
        y1 = (float)(view->y + view->height);
    } else {
        /* Map wl_output logical space into the EGL window (same as out_w * output_scale ≈ ctx->size). */
        float ow = (float)(out_w > 0 ? out_w : 1);
        float oh = (float)(out_h > 0 ? out_h : 1);
        x0 = (float)view->x / ow * (float)ctx->width;
        y0 = (float)view->y / oh * (float)ctx->height;
        x1 = (float)(view->x + view->width) / ow * (float)ctx->width;
        y1 = (float)(view->y + view->height) / oh * (float)ctx->height;
    }
    /* NDC from physical viewport (ctx) */
    float l = 2.0f * x0 / (float)ctx->width - 1.0f;
    float r = 2.0f * x1 / (float)ctx->width - 1.0f;
    float t = 1.0f - 2.0f * y0 / (float)ctx->height;
    float b = 1.0f - 2.0f * y1 / (float)ctx->height;
    float u0 = view->src_x, v0 = view->src_y;
    float u1 = view->src_x + view->src_w, v1 = view->src_y + view->src_h;
    GLfloat verts[] = {
        l, b, 0.f, u0, v1,  r, b, 0.f, u1, v1,
        l, t, 0.f, u0, v0,  r, t, 0.f, u1, v0
    };

    bool wl_shm_xrgb = view->pixels && view->format == WL_SHM_FORMAT_XRGB8888;
    GLuint tex_prog = wl_shm_xrgb ? ctx->tex_prog_rgbx : ctx->tex_prog_rgba;
    glUseProgram(tex_prog);
    glUniform1f(glGetUniformLocation(tex_prog, "u_swizzle"), swizzle);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx->tex_id);
    GLint apos = glGetAttribLocation(tex_prog, "a_position");
    GLint atc = glGetAttribLocation(tex_prog, "a_texcoord");
    glEnableVertexAttribArray(apos);
    glEnableVertexAttribArray(atc);
    glVertexAttribPointer(apos, 3, GL_FLOAT, GL_FALSE, 20, verts);
    glVertexAttribPointer(atc, 2, GL_FLOAT, GL_FALSE, 20, verts + 3);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(apos);
    glDisableVertexAttribArray(atc);

    if (egl_image_to_destroy && ctx->eglDestroyImageKHR)
        ctx->eglDestroyImageKHR(ctx->display, egl_image_to_destroy);
    return 0;
}

bool renderer_render(renderer_context_t *ctx, struct wayland_server *srv) {
    if (!ctx || !ctx->valid) return false;
    if (!eglMakeCurrent(ctx->display, ctx->surface, ctx->surface, ctx->context)) return false;
    {
        static int first_frame_logged;
        if (!first_frame_logged) {
            EGLint w = 0, h = 0;
            eglQuerySurface(ctx->display, ctx->surface, EGL_WIDTH, &w);
            eglQuerySurface(ctx->display, ctx->surface, EGL_HEIGHT, &h);
            LOGI("first EGL frame (compositing): surface %dx%d — TrierarchDmabuf/TrierarchRenderer detail only after clients attach buffers",
                    (int)w, (int)h);
            first_frame_logged = 1;
        }
    }
    EGLint w, h;
    eglQuerySurface(ctx->display, ctx->surface, EGL_WIDTH, &w);
    eglQuerySurface(ctx->display, ctx->surface, EGL_HEIGHT, &h);
    if (w != ctx->width || h != ctx->height) { ctx->width = w; ctx->height = h; }
    /* Logical size = physical * (resolution% * scale%) is set in JNI; we render to full screen */
    glViewport(0, 0, ctx->width, ctx->height);
    if (!ctx->bg_prog) ctx->bg_prog = make_program(vert_src, frag_bg_src);
    if (!ctx->bg_prog) return false;
    glUseProgram(ctx->bg_prog);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    GLfloat bg[] = { -1,-1,0, 1,-1,0, -1,1,0, 1,1,0 };
    GLint apos = glGetAttribLocation(ctx->bg_prog, "a_position");
    glEnableVertexAttribArray(apos);
    glVertexAttribPointer(apos, 3, GL_FLOAT, GL_FALSE, 0, bg);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(apos);
    if (srv) {
        int32_t out_w = 0, out_h = 0;
        compositor_get_output_size((wayland_server_t *)srv, &out_w, &out_h);
        draw_user_t ru = { ctx, out_w, out_h };
        glEnable(GL_BLEND);
        /* wlroots gles2_begin(): premultiplied surface color */
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        compositor_foreach_surface((wayland_server_t *)srv, draw_surface_cb, &ru);
        compositor_surface_view_t cursor_view;
        if (compositor_get_cursor_view((wayland_server_t *)srv, &cursor_view))
            draw_surface_cb(&cursor_view, &ru);
        glDisable(GL_BLEND);
    }
    eglSwapBuffers(ctx->display, ctx->surface);
    return true;
}
