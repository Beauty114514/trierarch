/*
 * zwp_linux_dmabuf_v1 (protocol v3): single-plane 32bpp RGBA/X formats.
 * KDE/KWin often submits DRM_FORMAT_ABGR8888 / XBGR8888 (GLES-friendly); we used to accept only
 * XRGB/ARGB + LINEAR|INVALID modifiers, which caused create_immed to fail → black screen.
 * Modifiers: accept vendor tiling/compression flags; renderer tries EGL import then mmap readback.
 */
#include "server_internal.h"
#include "linux-dmabuf-v1-server-protocol.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/log.h>

/* Defined in compositor.c; used to clear surface refs when a dmabuf wl_buffer is destroyed. */
extern struct wayland_server *g_wayland_server;

#define LOG_TAG "TrierarchDmabuf"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static void log_dmabuf_wl_buffer(const char *via, struct dmabuf_buffer *db) {
    static unsigned n;
    if (n >= 64)
        return;
    n++;
    LOGI("wl_buffer dmabuf [%s]: %dx%d fmt=0x%x stride=%u off=%u mod=0x%016llx",
            via, db->width, db->height, db->drm_format, db->stride, db->offset,
            (unsigned long long)db->modifier);
}

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
#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR 0ull
#endif
#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID 0x00ffffffffffffffull
#endif

struct params_state {
    bool used;
    bool plane0_set;
    int plane0_fd;
    uint32_t plane0_offset;
    uint32_t plane0_stride;
    uint64_t modifier;
};

static void params_state_reset(struct params_state *st) {
    if (st->plane0_fd >= 0) {
        close(st->plane0_fd);
        st->plane0_fd = -1;
    }
    st->plane0_set = false;
    st->plane0_offset = 0;
    st->plane0_stride = 0;
    st->modifier = 0;
}

static void dmabuf_wl_buffer_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct wl_buffer_interface dmabuf_buffer_iface = { .destroy = dmabuf_wl_buffer_destroy };

static void dmabuf_buffer_resource_destroy(struct wl_resource *resource) {
    struct dmabuf_buffer *db = wl_resource_get_user_data(resource);
    wl_resource_set_user_data(resource, NULL);
    if (!db) return;
    if (g_wayland_server) {
        pthread_mutex_lock(&g_wayland_server->surfaces_mutex);
        struct compositor_surface *surf;
        wl_list_for_each(surf, &g_wayland_server->surfaces, link) {
            if (surf->current_buffer && surf->current_buffer->type == BUF_DMABUF
                    && surf->current_buffer->u.dmabuf == db)
                surf->current_buffer->u.dmabuf = NULL;
            if (surf->pending_buffer && surf->pending_buffer->type == BUF_DMABUF
                    && surf->pending_buffer->u.dmabuf == db)
                surf->pending_buffer->u.dmabuf = NULL;
        }
        pthread_mutex_unlock(&g_wayland_server->surfaces_mutex);
    }
    if (db->dmabuf_fd >= 0) {
        close(db->dmabuf_fd);
        db->dmabuf_fd = -1;
    }
    db->resource = NULL;
    free(db);
}

static bool validate_fmt_mod(uint32_t format, uint64_t mod) {
    if (format != DRM_FORMAT_XRGB8888 && format != DRM_FORMAT_ARGB8888
            && format != DRM_FORMAT_XBGR8888 && format != DRM_FORMAT_ABGR8888)
        return false;
    /* Must match advertised modifiers in linux_dmabuf_bind (LINEAR only).
     * Previously this ignored `mod`, so clients could still attach UBWC/tiled
     * dmabufs while we only meant to allow LINEAR — mmap/EGL paths then showed
     * black.  Reject everything except DRM_FORMAT_MOD_LINEAR (0). */
    if (mod != DRM_FORMAT_MOD_LINEAR)
        return false;
    return true;
}

/* Build dmabuf_buffer from params; consumes plane0 fd on success (st->plane0_fd set -1). */
static struct dmabuf_buffer *params_to_dmabuf(struct params_state *st, int32_t width, int32_t height,
        uint32_t format) {
    if (!st->plane0_set || st->plane0_fd < 0) return NULL;
    if (width <= 0 || height <= 0) {
        LOGE("dmabuf reject: bad size %dx%d", width, height);
        close(st->plane0_fd);
        st->plane0_fd = -1;
        st->plane0_set = false;
        return NULL;
    }
    if (!validate_fmt_mod(format, st->modifier)) {
        LOGE("dmabuf reject: unsupported fmt=0x%x mod=0x%016llx (client will get failed/incomplete)",
                format, (unsigned long long)st->modifier);
        close(st->plane0_fd);
        st->plane0_fd = -1;
        st->plane0_set = false;
        return NULL;
    }
    struct dmabuf_buffer *db = calloc(1, sizeof(*db));
    if (!db) return NULL;
    db->magic = TRIERARCH_DMABUF_MAGIC;
    db->dmabuf_fd = st->plane0_fd;
    st->plane0_fd = -1;
    db->width = width;
    db->height = height;
    db->stride = st->plane0_stride;
    db->offset = st->plane0_offset;
    db->drm_format = format;
    db->modifier = st->modifier;
    db->owner = NULL;
    return db;
}

static int attach_buffer(struct wl_client *client, struct wl_resource *buf_res, struct dmabuf_buffer *db) {
    (void)client;
    db->resource = buf_res;
    wl_resource_set_implementation(buf_res, &dmabuf_buffer_iface, db, dmabuf_buffer_resource_destroy);
    return 0;
}

static void params_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void params_add(struct wl_client *client, struct wl_resource *resource,
        int32_t fd, uint32_t plane_idx, uint32_t offset, uint32_t stride,
        uint32_t modifier_hi, uint32_t modifier_lo) {
    (void)client;
    struct params_state *st = wl_resource_get_user_data(resource);
    if (!st || st->used) {
        if (fd >= 0) close(fd);
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "already used");
        return;
    }
    if (plane_idx != 0) {
        close(fd);
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, "plane");
        return;
    }
    if (st->plane0_set) {
        close(fd);
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET, "plane set");
        return;
    }
    st->plane0_fd = fd;
    st->plane0_offset = offset;
    st->plane0_stride = stride;
    st->modifier = ((uint64_t)modifier_hi << 32) | modifier_lo;
    st->plane0_set = true;
}

static void params_create(struct wl_client *client, struct wl_resource *resource,
        int32_t width, int32_t height, uint32_t format, uint32_t flags) {
    (void)flags;
    struct params_state *st = wl_resource_get_user_data(resource);
    if (!st || st->used) {
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "used");
        return;
    }
    struct dmabuf_buffer *db = params_to_dmabuf(st, width, height, format);
    if (!db) {
        if (st->plane0_fd >= 0) close(st->plane0_fd), st->plane0_fd = -1;
        zwp_linux_buffer_params_v1_send_failed(resource);
        return;
    }
    struct wl_resource *buf_res = wl_resource_create(client, &wl_buffer_interface, 1, 0);
    if (!buf_res) {
        close(db->dmabuf_fd);
        free(db);
        wl_client_post_no_memory(client);
        zwp_linux_buffer_params_v1_send_failed(resource);
        return;
    }
    if (attach_buffer(client, buf_res, db) != 0) {
        wl_resource_destroy(buf_res);
        zwp_linux_buffer_params_v1_send_failed(resource);
        return;
    }
    st->used = true;
    log_dmabuf_wl_buffer("create", db);
    zwp_linux_buffer_params_v1_send_created(resource, buf_res);
}

static void params_create_immed(struct wl_client *client, struct wl_resource *resource,
        uint32_t buffer_id, int32_t width, int32_t height, uint32_t format, uint32_t flags) {
    (void)flags;
    struct params_state *st = wl_resource_get_user_data(resource);
    if (!st || st->used) {
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "used");
        return;
    }
    struct dmabuf_buffer *db = params_to_dmabuf(st, width, height, format);
    if (!db) {
        if (st->plane0_fd >= 0) close(st->plane0_fd), st->plane0_fd = -1;
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE, "incomplete");
        return;
    }
    struct wl_resource *buf_res = wl_resource_create(client, &wl_buffer_interface, 1, buffer_id);
    if (!buf_res) {
        close(db->dmabuf_fd);
        free(db);
        wl_client_post_no_memory(client);
        return;
    }
    if (attach_buffer(client, buf_res, db) != 0) {
        wl_resource_destroy(buf_res);
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER, "buf");
        return;
    }
    st->used = true;
    log_dmabuf_wl_buffer("create_immed", db);
}

static void params_resource_deleted(struct wl_resource *resource) {
    struct params_state *st = wl_resource_get_user_data(resource);
    if (!st) return;
    if (!st->used)
        params_state_reset(st);
    free(st);
}

static const struct zwp_linux_buffer_params_v1_interface params_impl = {
    .destroy = params_destroy,
    .add = params_add,
    .create = params_create,
    .create_immed = params_create_immed,
};

static void dmabuf_create_params(struct wl_client *client, struct wl_resource *resource, uint32_t params_id) {
    struct params_state *st = calloc(1, sizeof(*st));
    if (!st) {
        wl_client_post_no_memory(client);
        return;
    }
    st->plane0_fd = -1;
    struct wl_resource *pr = wl_resource_create(client, &zwp_linux_buffer_params_v1_interface,
            wl_resource_get_version(resource), params_id);
    if (!pr) {
        free(st);
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(pr, &params_impl, st, params_resource_deleted);
}

static void dmabuf_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct zwp_linux_dmabuf_v1_interface dmabuf_impl = {
    .destroy = dmabuf_destroy,
    .create_params = dmabuf_create_params,
};

void linux_dmabuf_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    (void)data;
    if (version > 3) version = 3;
    struct wl_resource *res = wl_resource_create(client, &zwp_linux_dmabuf_v1_interface, version, id);
    if (!res) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(res, &dmabuf_impl, NULL, NULL);
    LOGI("bind zwp_linux_dmabuf_v1 version=%u (global max v3; no get_*_feedback)", version);
    zwp_linux_dmabuf_v1_send_format(res, DRM_FORMAT_XRGB8888);
    zwp_linux_dmabuf_v1_send_format(res, DRM_FORMAT_ARGB8888);
    zwp_linux_dmabuf_v1_send_format(res, DRM_FORMAT_XBGR8888);
    zwp_linux_dmabuf_v1_send_format(res, DRM_FORMAT_ABGR8888);
    /* LINEAR only: the mmap CPU-readback fallback in renderer.c reads linearly.
     * UBWC / tiled dmabufs are unmappable in linear mode and produce black or
     * garbage.  Advertising only LINEAR forces well-behaved clients (Mesa EGL
     * Wayland platform) to allocate linear buffers.
     * MOD_INVALID was previously advertised but allowed the GPU driver to pick
     * UBWC — can cause black screen with some guest GL stacks. */
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_XRGB8888, 0, 0);
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_ARGB8888, 0, 0);
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_XBGR8888, 0, 0);
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_ABGR8888, 0, 0);
}

struct dmabuf_buffer *dmabuf_buffer_try_from_wl_resource(struct wl_resource *buf_res) {
    if (!buf_res) return NULL;
    void *u = wl_resource_get_user_data(buf_res);
    if (!u) return NULL;
    struct dmabuf_buffer *db = u;
    if (db->magic != TRIERARCH_DMABUF_MAGIC) return NULL;
    return db;
}
