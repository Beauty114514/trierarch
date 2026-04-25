/*
 * zwp_linux_dmabuf_v1: single-plane 32bpp RGBA/X formats.
 * KDE/KWin often submits DRM_FORMAT_ABGR8888 / XBGR8888 (GLES-friendly); we used to accept only
 * XRGB/ARGB + LINEAR|INVALID modifiers, which caused create_immed to fail → black screen.
 * Modifiers: accept vendor tiling/compression flags; renderer tries EGL import then mmap readback.
 *
 * Also implements zwp_linux_dmabuf_feedback_v1 (since protocol v4). Mesa Vulkan Wayland WSI
 * uses dmabuf feedback for format/modifier negotiation and presentation support checks.
 */
#include "server_internal.h"
#include "linux-dmabuf-v1-server-protocol.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/syscall.h>
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

/* linux-dmabuf v4 feedback format table entry (see protocol XML). */
struct trierarch_dmabuf_format_table_entry {
    uint32_t format;
    uint32_t padding;
    uint64_t modifier;
};

#ifndef SYS_memfd_create
#if defined(__aarch64__)
#define SYS_memfd_create 279
#elif defined(__arm__)
#define SYS_memfd_create 385
#endif
#endif

static int trierarch_memfd_create(const char *name) {
#ifdef SYS_memfd_create
    return (int)syscall(SYS_memfd_create, name, 0);
#else
    (void)name;
    errno = ENOSYS;
    return -1;
#endif
}

static void fill_device_id_from_path(const char *path, struct wl_array *out) {
    struct stat st;
    if (stat(path, &st) != 0)
        return;
    wl_array_add(out, sizeof(st.st_rdev));
    memcpy(out->data, &st.st_rdev, sizeof(st.st_rdev));
}

static void dmabuf_feedback_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct zwp_linux_dmabuf_feedback_v1_interface dmabuf_feedback_impl = {
    .destroy = dmabuf_feedback_destroy,
};

/* DRM format / modifier constants used by both feedback and bind.
 * Defined here early so all functions below can use them. */
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

static void send_dmabuf_feedback(struct wl_resource *feedback_res) {
    /* Format table.
     *
     * Important: Mesa (incl. zink) may insist on dmabuf-feedback (linux-dmabuf v4).
     * If we provide feedback but only list LINEAR, some stacks still refuse dmabuf
     * unless MOD_INVALID (implicit modifier) is also present.
     *
     * We advertise both LINEAR and MOD_INVALID for the common 32bpp formats. */
    struct trierarch_dmabuf_format_table_entry entries[8];
    memset(entries, 0, sizeof(entries));
    entries[0].format = DRM_FORMAT_XRGB8888; entries[0].modifier = DRM_FORMAT_MOD_LINEAR;
    entries[1].format = DRM_FORMAT_ARGB8888; entries[1].modifier = DRM_FORMAT_MOD_LINEAR;
    entries[2].format = DRM_FORMAT_XBGR8888; entries[2].modifier = DRM_FORMAT_MOD_LINEAR;
    entries[3].format = DRM_FORMAT_ABGR8888; entries[3].modifier = DRM_FORMAT_MOD_LINEAR;
    entries[4].format = DRM_FORMAT_XRGB8888; entries[4].modifier = DRM_FORMAT_MOD_INVALID;
    entries[5].format = DRM_FORMAT_ARGB8888; entries[5].modifier = DRM_FORMAT_MOD_INVALID;
    entries[6].format = DRM_FORMAT_XBGR8888; entries[6].modifier = DRM_FORMAT_MOD_INVALID;
    entries[7].format = DRM_FORMAT_ABGR8888; entries[7].modifier = DRM_FORMAT_MOD_INVALID;

    int table_fd = trierarch_memfd_create("trierarch-dmabuf-table");
    if (table_fd >= 0) {
        (void)ftruncate(table_fd, (off_t)sizeof(entries));
        (void)write(table_fd, entries, sizeof(entries));
        lseek(table_fd, 0, SEEK_SET);
        zwp_linux_dmabuf_feedback_v1_send_format_table(feedback_res, table_fd, (uint32_t)sizeof(entries));
        /* fd is transferred; close our copy */
        close(table_fd);
    } else {
        LOGE("dmabuf feedback: memfd_create failed (%d), sending no format_table", errno);
    }

    struct wl_array dev;
    wl_array_init(&dev);
    /* main_device must be a DRM render-node dev_t: nested compositors (KWin,
     * wlroots) call drmGetDeviceFromDevId(), which cannot resolve KGSL. Use a
     * render node only; if none is visible in this environment, send dev_t 0. */
    fill_device_id_from_path("/dev/dri/renderD128", &dev);
    if (dev.size == 0) {
        dev_t zero = 0;
        wl_array_add(&dev, sizeof(zero));
        memcpy(dev.data, &zero, sizeof(zero));
    }

    zwp_linux_dmabuf_feedback_v1_send_main_device(feedback_res, &dev);
    zwp_linux_dmabuf_feedback_v1_send_tranche_target_device(feedback_res, &dev);
    zwp_linux_dmabuf_feedback_v1_send_tranche_flags(feedback_res, 0);

    struct wl_array idx;
    wl_array_init(&idx);
    uint16_t *p = wl_array_add(&idx, 8 * sizeof(uint16_t));
    if (p) {
        for (uint16_t i = 0; i < 8; i++)
            p[i] = i;
        zwp_linux_dmabuf_feedback_v1_send_tranche_formats(feedback_res, &idx);
    }
    zwp_linux_dmabuf_feedback_v1_send_tranche_done(feedback_res);
    zwp_linux_dmabuf_feedback_v1_send_done(feedback_res);

    wl_array_release(&idx);
    wl_array_release(&dev);
}

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
    /* Must match advertised modifiers in linux_dmabuf_bind.
     *
     * Why allow MOD_INVALID?
     * - Some Mesa stacks refuse dmabuf unless MOD_INVALID is present (implicit modifier).
     * - We still prefer LINEAR (mmap fallback assumes linear), but MOD_INVALID can
     *   still represent a linear allocation on some devices.
     *
     * Note: If the driver chooses a tiled/compressed layout under MOD_INVALID, our
     * mmap path may show black/garbage; that's still better than forcing SHM where
     * some clients (zink/glmark2) currently submit all-zero buffers. */
    if (mod != DRM_FORMAT_MOD_LINEAR && mod != DRM_FORMAT_MOD_INVALID)
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
    static unsigned add_logged;
    if (!st || st->used) {
        if (fd >= 0) close(fd);
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "already used");
        return;
    }
    if (plane_idx != 0) {
        if (add_logged < 32) {
            LOGE("dmabuf params_add reject: plane_idx=%u (only plane0 supported)", plane_idx);
            add_logged++;
        }
        close(fd);
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX, "plane");
        return;
    }
    if (st->plane0_set) {
        if (add_logged < 32) {
            LOGE("dmabuf params_add reject: plane already set");
            add_logged++;
        }
        close(fd);
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET, "plane set");
        return;
    }
    st->plane0_fd = fd;
    st->plane0_offset = offset;
    st->plane0_stride = stride;
    st->modifier = ((uint64_t)modifier_hi << 32) | modifier_lo;
    st->plane0_set = true;
    if (add_logged < 48) {
        LOGI("dmabuf params_add: fd=%d off=%u stride=%u mod=0x%016llx",
                fd, offset, stride, (unsigned long long)st->modifier);
        add_logged++;
    }
}

static void params_create(struct wl_client *client, struct wl_resource *resource,
        int32_t width, int32_t height, uint32_t format, uint32_t flags) {
    (void)flags;
    struct params_state *st = wl_resource_get_user_data(resource);
    static unsigned create_logged;
    if (!st || st->used) {
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "used");
        return;
    }
    struct dmabuf_buffer *db = params_to_dmabuf(st, width, height, format);
    if (!db) {
        if (create_logged < 64) {
            LOGE("dmabuf create FAILED: %dx%d fmt=0x%x (see reject above)", width, height, format);
            create_logged++;
        }
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
    if (create_logged < 64) {
        LOGI("dmabuf create OK: wl_buffer created (fmt=0x%x mod=0x%016llx)", db->drm_format, (unsigned long long)db->modifier);
        create_logged++;
    }
    zwp_linux_buffer_params_v1_send_created(resource, buf_res);
}

static void params_create_immed(struct wl_client *client, struct wl_resource *resource,
        uint32_t buffer_id, int32_t width, int32_t height, uint32_t format, uint32_t flags) {
    (void)flags;
    struct params_state *st = wl_resource_get_user_data(resource);
    static unsigned immed_logged;
    if (!st || st->used) {
        wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED, "used");
        return;
    }
    struct dmabuf_buffer *db = params_to_dmabuf(st, width, height, format);
    if (!db) {
        if (immed_logged < 64) {
            LOGE("dmabuf create_immed FAILED: id=%u %dx%d fmt=0x%x (see reject above)",
                    buffer_id, width, height, format);
            immed_logged++;
        }
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
    if (immed_logged < 64) {
        LOGI("dmabuf create_immed OK: wl_buffer id=%u fmt=0x%x mod=0x%016llx",
                buffer_id, db->drm_format, (unsigned long long)db->modifier);
        immed_logged++;
    }
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

static void dmabuf_get_default_feedback(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    (void)resource;
    struct wl_resource *fb = wl_resource_create(client, &zwp_linux_dmabuf_feedback_v1_interface, 1, id);
    if (!fb) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(fb, &dmabuf_feedback_impl, NULL, NULL);
    send_dmabuf_feedback(fb);
}

static void dmabuf_get_surface_feedback(struct wl_client *client, struct wl_resource *resource, uint32_t id,
        struct wl_resource *surface) {
    (void)surface;
    dmabuf_get_default_feedback(client, resource, id);
}

static const struct zwp_linux_dmabuf_v1_interface dmabuf_impl = {
    .destroy = dmabuf_destroy,
    .create_params = dmabuf_create_params,
    .get_default_feedback = dmabuf_get_default_feedback,
    .get_surface_feedback = dmabuf_get_surface_feedback,
};

void linux_dmabuf_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    (void)data;
    /* Mesa (incl. zink) can require linux-dmabuf v4 for dmabuf feedback. */
    if (version > 4) version = 4;
    struct wl_resource *res = wl_resource_create(client, &zwp_linux_dmabuf_v1_interface, version, id);
    if (!res) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(res, &dmabuf_impl, NULL, NULL);
    LOGI("bind zwp_linux_dmabuf_v1 version=%u (global max v4)", version);
    zwp_linux_dmabuf_v1_send_format(res, DRM_FORMAT_XRGB8888);
    zwp_linux_dmabuf_v1_send_format(res, DRM_FORMAT_ARGB8888);
    zwp_linux_dmabuf_v1_send_format(res, DRM_FORMAT_XBGR8888);
    zwp_linux_dmabuf_v1_send_format(res, DRM_FORMAT_ABGR8888);
    /* Prefer LINEAR; also advertise MOD_INVALID (implicit modifier) for Mesa stacks
     * that won't use dmabuf otherwise. */
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_XRGB8888, 0, 0);
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_ARGB8888, 0, 0);
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_XBGR8888, 0, 0);
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_ABGR8888, 0, 0);
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_XRGB8888,
            (uint32_t)(DRM_FORMAT_MOD_INVALID >> 32), (uint32_t)DRM_FORMAT_MOD_INVALID);
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_ARGB8888,
            (uint32_t)(DRM_FORMAT_MOD_INVALID >> 32), (uint32_t)DRM_FORMAT_MOD_INVALID);
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_XBGR8888,
            (uint32_t)(DRM_FORMAT_MOD_INVALID >> 32), (uint32_t)DRM_FORMAT_MOD_INVALID);
    zwp_linux_dmabuf_v1_send_modifier(res, DRM_FORMAT_ABGR8888,
            (uint32_t)(DRM_FORMAT_MOD_INVALID >> 32), (uint32_t)DRM_FORMAT_MOD_INVALID);
}

struct dmabuf_buffer *dmabuf_buffer_try_from_wl_resource(struct wl_resource *buf_res) {
    if (!buf_res) return NULL;
    void *u = wl_resource_get_user_data(buf_res);
    if (!u) return NULL;
    struct dmabuf_buffer *db = u;
    if (db->magic != TRIERARCH_DMABUF_MAGIC) return NULL;
    return db;
}
