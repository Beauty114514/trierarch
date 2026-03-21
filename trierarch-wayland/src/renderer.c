/*
 * EGL/GLES2: SHM texture + EGL Wayland buffer import for KDE/llvmpipe.
 */
#include "renderer.h"
#include "compositor.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "TrierarchRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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
    GLuint tex_prog, bg_prog, tex_id;
    PFN_eglQueryWaylandBufferWL eglQueryWaylandBufferWL;
    PFN_eglCreateImageKHR eglCreateImageKHR;
    PFN_eglDestroyImageKHR eglDestroyImageKHR;
    PFN_glEGLImageTargetTexture2DOES glEGLImageTargetTexture2DOES;
};

static const char *vert_src = "attribute vec4 a_position; void main() { gl_Position = a_position; }\n";
static const char *vert_tex_src = "attribute vec4 a_position; attribute vec2 a_texcoord; varying vec2 v_texcoord; void main() { gl_Position = a_position; v_texcoord = a_texcoord; }\n";
static const char *frag_bg_src = "precision mediump float; void main() { gl_FragColor = vec4(0.0,0.0,0.0,1.0); }\n";
/* u_swizzle=1: SHM ARGB8888 BGRA bytes; u_swizzle=0: EGL buffer already RGBA */
static const char *frag_tex_src = "precision mediump float; varying vec2 v_texcoord; uniform sampler2D u_tex; uniform float u_swizzle; void main() { vec4 c = texture2D(u_tex, v_texcoord); vec4 bgra = vec4(c.b, c.g, c.r, c.a); gl_FragColor = mix(c, bgra, u_swizzle); }\n";

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

renderer_context_t *renderer_create(ANativeWindow *window, struct wayland_server *srv) {
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
    if (srv && ctx->eglQueryWaylandBufferWL && ctx->eglCreateImageKHR && ctx->glEGLImageTargetTexture2DOES) {
        PFN_eglBindWaylandDisplayWL bind_wl = (PFN_eglBindWaylandDisplayWL)eglGetProcAddress("eglBindWaylandDisplayWL");
        void *wl_display = compositor_get_wl_display((wayland_server_t *)srv);
        if (bind_wl && wl_display && bind_wl(ctx->display, wl_display)) {
            compositor_set_egl_buffer_supported((wayland_server_t *)srv, true);
            LOGI("eglBindWaylandDisplayWL OK, EGL buffer import enabled");
        }
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
    if (ctx->tex_prog) { glDeleteProgram(ctx->tex_prog); ctx->tex_prog = 0; }
    if (ctx->bg_prog) { glDeleteProgram(ctx->bg_prog); ctx->bg_prog = 0; }
    eglMakeCurrent(ctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool renderer_is_valid(renderer_context_t *ctx) { return ctx && ctx->valid; }
void renderer_get_size(renderer_context_t *ctx, int *w, int *h) { if (ctx && w) *w = ctx->width; if (ctx && h) *h = ctx->height; }

typedef struct { renderer_context_t *ctx; int32_t scale; int32_t out_w; int32_t out_h; } draw_user_t;

static int draw_surface_cb(const compositor_surface_view_t *view, void *user) {
    draw_user_t *ru = user;
    renderer_context_t *ctx = ru->ctx;
    if (!view || (view->pixels == NULL && view->egl_buffer == NULL) || view->width <= 0 || view->height <= 0)
        return 0;
    if (!ctx->tex_prog) {
        ctx->tex_prog = make_program(vert_tex_src, frag_tex_src);
        glGenTextures(1, &ctx->tex_id);
        if (!ctx->tex_prog || !ctx->tex_id) return 1;
    }

    int buf_w = view->buf_width > 0 ? view->buf_width : view->width;
    int buf_h = view->buf_height > 0 ? view->buf_height : view->height;
    void *egl_image_to_destroy = NULL;
    float swizzle = 1.0f;

    if (view->egl_buffer && ctx->eglQueryWaylandBufferWL && ctx->eglCreateImageKHR && ctx->glEGLImageTargetTexture2DOES) {
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
    }

    int32_t scale = ru->scale > 0 ? ru->scale : 1;
    int32_t out_w = ru->out_w > 0 ? ru->out_w : ctx->width;
    int32_t out_h = ru->out_h > 0 ? ru->out_h : ctx->height;
    float x0, y0, x1, y1;
    if (view->position_in_physical) {
        /* Cursor: position in physical px; size scaled by output_scale (logical→physical fill) */
        x0 = (float)view->x;
        y0 = (float)view->y;
        x1 = (float)(view->x + view->width * scale);
        y1 = (float)(view->y + view->height * scale);
    } else {
        x0 = (float)(view->x * scale);
        y0 = (float)(view->y * scale);
        x1 = (float)((view->x + view->width) * scale);
        y1 = (float)((view->y + view->height) * scale);
        if (view->x == 0 && view->y == 0 && view->width == out_w && view->height == out_h) {
            x0 = 0; y0 = 0; x1 = (float)ctx->width; y1 = (float)ctx->height;
        }
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

    glUseProgram(ctx->tex_prog);
    glUniform1f(glGetUniformLocation(ctx->tex_prog, "u_swizzle"), swizzle);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx->tex_id);
    GLint apos = glGetAttribLocation(ctx->tex_prog, "a_position");
    GLint atc = glGetAttribLocation(ctx->tex_prog, "a_texcoord");
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
        int32_t scale = compositor_get_output_scale((wayland_server_t *)srv);
        draw_user_t ru = { ctx, scale, out_w, out_h };
        glEnable(GL_BLEND);
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
