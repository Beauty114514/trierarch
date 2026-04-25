/*
 * wl_compositor, wl_surface, wl_region. Attach/commit/damage/frame.
 */
#include "server_internal.h"
#include <stdlib.h>
#include <android/log.h>
#include <wayland-server-protocol.h>
#include "fractional-scale-v1-server-protocol.h"

#define LOG_TAG "TrierarchSurface"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* When eglBindWaylandDisplayWL was not called, egl_buffer_supported is false and
 * wl_buffer with NULL user_data cannot be attached — every frame is dropped. */
static uint64_t g_wl_buffer_dropped_no_egl_import;

static void xdg_surface_resource_destroy(struct wl_resource *resource);
static void xdg_toplevel_resource_destroy(struct wl_resource *resource);

static void surface_resource_destroy(struct wl_resource *resource) {
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    if (!surf) return;
    if (surf->xdg_toplevel_res) {
        wl_resource_set_user_data(surf->xdg_toplevel_res, NULL);
        surf->xdg_toplevel_res = NULL;
    }
    if (surf->xdg_surface_res) {
        wl_resource_set_user_data(surf->xdg_surface_res, NULL);
        surf->xdg_surface_res = NULL;
    }
    if (surf->subsurface_res) {
        wl_resource_set_user_data(surf->subsurface_res, NULL);
        surf->subsurface_res = NULL;
    }
    if (surf->current_buffer && surf->current_buffer->type == BUF_EGL && surf->current_buffer->u.egl)
        surf->current_buffer->u.egl->surf = NULL;
    if (surf->pending_buffer && surf->pending_buffer->type == BUF_EGL && surf->pending_buffer->u.egl)
        surf->pending_buffer->u.egl->surf = NULL;
    if (surf->current_buffer && surf->current_buffer->type == BUF_DMABUF && surf->current_buffer->u.dmabuf)
        surf->current_buffer->u.dmabuf->owner = NULL;
    if (surf->pending_buffer && surf->pending_buffer->type == BUF_DMABUF && surf->pending_buffer->u.dmabuf)
        surf->pending_buffer->u.dmabuf->owner = NULL;
    if (surf->parent) {
        surf->parent = NULL;
        wl_list_remove(&surf->sub_link);
        wl_list_init(&surf->sub_link);
    }
    pthread_mutex_lock(&surf->srv->surfaces_mutex);
    if (surf->srv->pointer_focus == surf)
        surf->srv->pointer_focus = NULL;
    if (surf->srv->cursor_surface == surf)
        surf->srv->cursor_surface = NULL;
    wl_list_remove(&surf->link);
    pthread_mutex_unlock(&surf->srv->surfaces_mutex);
    if (surf->current_buffer) {
        buffer_ref_release_no_post(surf->current_buffer);
        surf->current_buffer = NULL;
    }
    if (surf->pending_buffer) {
        buffer_ref_clear_owner(surf->pending_buffer);
        buffer_ref_release_no_post(surf->pending_buffer);
        surf->pending_buffer = NULL;
    }
    free(surf);
}

static void surface_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void surface_attach(struct wl_client *client, struct wl_resource *resource,
        struct wl_resource *buffer_res, int32_t x, int32_t y) {
    (void)client;
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    if (!surf) return;
    surf->buffer_offset_x = x;
    surf->buffer_offset_y = y;

    if (!buffer_res) {
        if (surf->pending_buffer) {
            buffer_ref_clear_owner(surf->pending_buffer);
            buffer_ref_release(surf->pending_buffer);
            surf->pending_buffer = NULL;
        }
        return;
    }
    {
        struct dmabuf_buffer *dba = dmabuf_buffer_try_from_wl_resource(buffer_res);
        if (dba) {
            struct compositor_buffer_ref *ref = calloc(1, sizeof(*ref));
            if (!ref) {
                wl_client_post_no_memory(client);
                return;
            }
            ref->type = BUF_DMABUF;
            ref->u.dmabuf = dba;
            if (surf->pending_buffer) {
                buffer_ref_clear_owner(surf->pending_buffer);
                buffer_ref_release(surf->pending_buffer);
            }
            surf->pending_buffer = ref;
            return;
        }
    }
    {
        struct ahb_buffer *ab = ahb_buffer_try_from_wl_resource(buffer_res);
        if (ab) {
            struct compositor_buffer_ref *ref = calloc(1, sizeof(*ref));
            if (!ref) {
                wl_client_post_no_memory(client);
                return;
            }
            ref->type = BUF_AHB;
            ref->u.ahb = ab;
            if (surf->pending_buffer) {
                buffer_ref_clear_owner(surf->pending_buffer);
                buffer_ref_release(surf->pending_buffer);
            }
            surf->pending_buffer = ref;
            return;
        }
    }
    void *raw = wl_resource_get_user_data(buffer_res);
    if (!raw) {
        /* EGL buffer (no user_data) */
        if (!surf->srv->egl_buffer_supported) {
            g_wl_buffer_dropped_no_egl_import++;
            if (g_wl_buffer_dropped_no_egl_import <= 15u
                    || (g_wl_buffer_dropped_no_egl_import % 600u) == 0u) {
                LOGE("wl_buffer attach IMPOSSIBLE: no user_data and EGL Wayland import disabled "
                     "(need eglBindWaylandDisplayWL on compositor). drop_count=%llu — frames are discarded.",
                     (unsigned long long)g_wl_buffer_dropped_no_egl_import);
            }
            wl_resource_post_event(buffer_res, WL_BUFFER_RELEASE);
            return;
        }
        int32_t w = 0, h = 0;
        if (surf->pending_buffer && buffer_ref_width(surf->pending_buffer) > 0)
            w = buffer_ref_width(surf->pending_buffer), h = buffer_ref_height(surf->pending_buffer);
        if ((w <= 0 || h <= 0) && surf->current_buffer && buffer_ref_width(surf->current_buffer) > 0)
            w = buffer_ref_width(surf->current_buffer), h = buffer_ref_height(surf->current_buffer);
        if (w <= 0 || h <= 0) {
            w = surf->srv->output_width > 0 ? surf->srv->output_width : 1080;
            h = surf->srv->output_height > 0 ? surf->srv->output_height : 1920;
        }
        struct compositor_buffer_ref *ref = buffer_attach_egl_buffer(client, buffer_res, surf, w, h);
        if (!ref) {
            wl_client_post_no_memory(client);
            return;
        }
        if (surf->pending_buffer) {
            buffer_ref_clear_owner(surf->pending_buffer);
            buffer_ref_release(surf->pending_buffer);
        }
        surf->pending_buffer = ref;
        return;
    }
    struct shm_buffer *buf = raw;
    struct compositor_buffer_ref *ref = calloc(1, sizeof(*ref));
    if (!ref) {
        wl_client_post_no_memory(client);
        return;
    }
    ref->type = BUF_SHM;
    ref->u.shm = buf;
    if (surf->pending_buffer) {
        buffer_ref_clear_owner(surf->pending_buffer);
        buffer_ref_release(surf->pending_buffer);
    }
    surf->pending_buffer = ref;
}

static void surface_damage(struct wl_client *client, struct wl_resource *resource,
        int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)client; (void)resource; (void)x; (void)y; (void)width; (void)height;
}

static void surface_damage_buffer(struct wl_client *client, struct wl_resource *resource,
        int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)client; (void)resource; (void)x; (void)y; (void)width; (void)height;
}

static void pending_frame_cb_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct pending_frame_cb *p = wl_container_of(listener, p, resource_listener);
    wl_list_remove(&p->link);
    wl_list_remove(&p->resource_listener.link);
    free(p);
}

static void surface_frame(struct wl_client *client, struct wl_resource *surface_res, uint32_t callback_id) {
    (void)surface_res;
    struct compositor_surface *surf = wl_resource_get_user_data(surface_res);
    if (!surf || !surf->srv) return;
    struct wl_resource *cb = wl_resource_create(client, &wl_callback_interface, 1, callback_id);
    if (cb) {
        struct pending_frame_cb *p = calloc(1, sizeof(*p));
        if (p) {
            p->resource = cb;
            p->resource_listener.notify = pending_frame_cb_destroy;
            wl_resource_add_destroy_listener(cb, &p->resource_listener);
            wl_resource_set_implementation(cb, NULL, NULL, NULL);
            wl_list_insert(surf->srv->pending_frame_callbacks.prev, &p->link);
        } else {
            wl_resource_destroy(cb);
        }
    }
}

static void surface_set_opaque_region(struct wl_client *c, struct wl_resource *r, struct wl_resource *region) {
    (void)c; (void)r; (void)region;
}
static void surface_set_input_region(struct wl_client *c, struct wl_resource *r, struct wl_resource *region) {
    (void)c; (void)r; (void)region;
}
static void surface_set_buffer_transform(struct wl_client *c, struct wl_resource *r, int32_t transform) {
    (void)c; (void)r; (void)transform;
}
static void surface_set_buffer_scale(struct wl_client *c, struct wl_resource *r, int32_t scale) {
    (void)c;
    struct compositor_surface *surf = wl_resource_get_user_data(r);
    if (!surf || scale < 1) return;
    /* KWin may send the same scale every frame; avoid log spam (logcat sync I/O can stall the server). */
    if (surf->buffer_scale == scale) return;
    surf->buffer_scale = scale;
    LOGI("surface=%p set_buffer_scale=%d", (void *)surf, (int)scale);
}

static void surface_commit(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    if (!surf) return;

    pthread_mutex_lock(&surf->srv->surfaces_mutex);
    struct compositor_buffer_ref *pending = surf->pending_buffer;
    surf->pending_buffer = NULL;

    if (pending && buffer_ref_width(pending) > 0 && buffer_ref_height(pending) > 0) {
        if (surf->current_buffer) {
            buffer_ref_clear_owner(surf->current_buffer);
            buffer_ref_release(surf->current_buffer);
        }
        surf->current_buffer = pending;
        buffer_ref_set_owner(pending, surf);
    } else {
        if (pending) buffer_ref_release(pending);
    }
    pthread_mutex_unlock(&surf->srv->surfaces_mutex);

    if (surf->current_buffer) {
        int32_t bw = buffer_ref_width(surf->current_buffer);
        int32_t bh = buffer_ref_height(surf->current_buffer);
        int32_t bs = surf->buffer_scale > 0 ? surf->buffer_scale : 1;
        const char *kind = "?";
        uint32_t fmt = 0;
        switch (surf->current_buffer->type) {
        case BUF_SHM: kind = "shm"; break;
        case BUF_DMABUF: kind = "dmabuf"; break;
        case BUF_EGL: kind = "egl"; break;
        default: break;
        }
        fmt = buffer_ref_format(surf->current_buffer);
        /* High-frequency commits + LOGI can block the Wayland thread and contribute to black screens. */
        static uint32_t g_commit_log_n;
        g_commit_log_n++;
        int do_log = (g_commit_log_n <= 24u) || ((g_commit_log_n & 255u) == 0u);
        if (do_log) {
            if (surf->viewport_dst_set)
                LOGI("surface=%p commit kind=%s fmt=0x%x buf=%dx%d scale=%d vp_dst=%dx%d (n=%u)",
                        (void *)surf, kind, (unsigned)fmt, (int)bw, (int)bh, (int)bs,
                        (int)surf->viewport_dst_w, (int)surf->viewport_dst_h, g_commit_log_n);
            else
                LOGI("surface=%p commit kind=%s fmt=0x%x buf=%dx%d scale=%d vp_dst=- (n=%u)",
                        (void *)surf, kind, (unsigned)fmt, (int)bw, (int)bh, (int)bs, g_commit_log_n);
        }
    }

    /* Send wl_surface.enter(output) when surface gets a buffer and hasn't yet */
    if (surf->current_buffer && !surf->entered_output) {
        struct wl_client *cl = wl_resource_get_client(surf->resource);
        struct output_resource_node *onode;
        wl_list_for_each(onode, &surf->srv->output_resources, link) {
            if (wl_resource_get_client(onode->resource) == cl) {
                wl_surface_send_enter(surf->resource, onode->resource);
                /* Encourage clients (esp. nested compositors) to allocate HiDPI buffers. */
                uint32_t surf_ver = (uint32_t)wl_resource_get_version(surf->resource);
                if (surf_ver >= 6) {
                    int32_t preferred = surf->srv && surf->srv->output_user_scale > 0 ? surf->srv->output_user_scale : 1;
                    if (preferred < 1) preferred = 1;
                    wl_surface_send_preferred_buffer_scale(surf->resource, preferred);
                }
                break;
            }
        }
        /* Also encourage fractional-scale clients (KWin) if bound. */
        if (surf->fractional_scale_res && surf->srv) {
            int32_t s = surf->srv->output_user_scale > 0 ? surf->srv->output_user_scale : 1;
            if (s < 1) s = 1;
            if (s > 4) s = 4;
            wp_fractional_scale_v1_send_preferred_scale(surf->fractional_scale_res, (uint32_t)(s * 120));
        }
        surf->entered_output = true;
    }
}

void surface_notify_preferred_buffer_scale_all(struct wayland_server *srv) {
    if (!srv) return;
    int32_t preferred = srv->output_user_scale > 0 ? srv->output_user_scale : 1;
    if (preferred < 1) preferred = 1;
    pthread_mutex_lock(&srv->surfaces_mutex);
    struct compositor_surface *surf;
    wl_list_for_each(surf, &srv->surfaces, link) {
        if (!surf->resource) continue;
        uint32_t ver = (uint32_t)wl_resource_get_version(surf->resource);
        if (ver < 6) continue;
        wl_surface_send_preferred_buffer_scale(surf->resource, preferred);
    }
    pthread_mutex_unlock(&srv->surfaces_mutex);
}

static const struct wl_surface_interface surface_impl = {
    .destroy = surface_destroy,
    .attach = surface_attach,
    .damage = surface_damage,
    .damage_buffer = surface_damage_buffer,
    .frame = surface_frame,
    .set_opaque_region = surface_set_opaque_region,
    .set_input_region = surface_set_input_region,
    .commit = surface_commit,
    .set_buffer_transform = surface_set_buffer_transform,
    .set_buffer_scale = surface_set_buffer_scale,
};

static void region_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}
static void region_add(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y, int32_t w, int32_t h) {
    (void)c;(void)r;(void)x;(void)y;(void)w;(void)h;
}
static void region_subtract(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y, int32_t w, int32_t h) {
    (void)c;(void)r;(void)x;(void)y;(void)w;(void)h;
}

static const struct wl_region_interface region_impl = {
    .destroy = region_destroy,
    .add = region_add,
    .subtract = region_subtract,
};

static void compositor_create_surface(struct wl_client *client,
        struct wl_resource *resource, uint32_t id) {
    struct wayland_server *srv = wl_resource_get_user_data(resource);
    struct compositor_surface *surf = calloc(1, sizeof(*surf));
    if (!surf) {
        wl_client_post_no_memory(client);
        return;
    }
    {
        static unsigned g_create_surf_logged;
        if (g_create_surf_logged < 64) {
            pid_t pid = (pid_t)-1;
            uid_t uid = 0;
            gid_t gid = 0;
            wl_client_get_credentials(client, &pid, &uid, &gid);
            LOGI("wl_compositor.create_surface: surf=%p pid=%d uid=%u (n=%u)",
                    (void *)surf, (int)pid, (unsigned)uid, g_create_surf_logged + 1);
            g_create_surf_logged++;
        }
    }
    surf->srv = srv;
    surf->current_buffer = NULL;
    surf->pending_buffer = NULL;
    surf->xdg_surface_res = NULL;
    surf->xdg_toplevel_res = NULL;
    surf->buffer_offset_x = surf->buffer_offset_y = 0;
    surf->buffer_scale = 1;
    surf->entered_output = false;
    surf->viewport_res = NULL;
    surf->viewport_dst_set = false;
    surf->viewport_dst_w = 0;
    surf->viewport_dst_h = 0;
    surf->viewport_src_set = false;
    surf->viewport_src_x = 0;
    surf->viewport_src_y = 0;
    surf->viewport_src_w = 0;
    surf->viewport_src_h = 0;
    surf->parent = NULL;
    surf->subsurface_res = NULL;
    wl_list_init(&surf->children);
    wl_list_init(&surf->sub_link);
    pthread_mutex_lock(&srv->surfaces_mutex);
    wl_list_insert(&srv->surfaces, &surf->link);
    pthread_mutex_unlock(&srv->surfaces_mutex);

    struct wl_resource *surface = wl_resource_create(client, &wl_surface_interface,
            wl_resource_get_version(resource), id);
    if (!surface) {
        pthread_mutex_lock(&srv->surfaces_mutex);
        wl_list_remove(&surf->link);
        pthread_mutex_unlock(&srv->surfaces_mutex);
        free(surf);
        wl_client_post_no_memory(client);
        return;
    }
    surf->resource = surface;
    wl_resource_set_implementation(surface, &surface_impl, surf, surface_resource_destroy);
}

static void compositor_create_region(struct wl_client *client,
        struct wl_resource *resource, uint32_t id) {
    struct wl_resource *region = wl_resource_create(client, &wl_region_interface,
            wl_resource_get_version(resource), id);
    if (!region) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(region, &region_impl, NULL, NULL);
}

static const struct wl_compositor_interface compositor_impl = {
    .create_surface = compositor_create_surface,
    .create_region = compositor_create_region,
};

void surface_compositor_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct wl_resource *resource = wl_resource_create(client, &wl_compositor_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &compositor_impl, data, NULL);
    {
        static unsigned g_bind_logged;
        if (g_bind_logged < 128) {
            pid_t pid = (pid_t)-1;
            uid_t uid = 0;
            gid_t gid = 0;
            wl_client_get_credentials(client, &pid, &uid, &gid);
            LOGI("bind wl_compositor v%u pid=%d uid=%u (n=%u)", version, (int)pid, (unsigned)uid, g_bind_logged + 1);
            g_bind_logged++;
        }
    }
}
