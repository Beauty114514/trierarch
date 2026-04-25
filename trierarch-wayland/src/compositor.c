/*
 * Wayland server core (Trierarch-owned compositor).
 *
 * Responsibilities:
 * - Create/destroy the wl_display and global interfaces.
 * - Maintain server-side state (surfaces, focus, output, cursor).
 * - Drive dispatch from either a background thread or the render loop (see `jni_bridge.c`).
 *
 * Contract:
 * - Socket name is fixed: `wayland-trierarch` under `XDG_RUNTIME_DIR` provided by the app.
 * - This compositor targets a "single fullscreen desktop" model: focus/targeting heuristics
 *   pick one best toplevel surface for input/render ordering.
 */
#include "server_internal.h"
#include <wayland-util.h>
#include "xdg-shell-server-protocol.h"
#include "single-pixel-buffer-v1-server-protocol.h"
#include "viewporter-server-protocol.h"
#include "xdg-decoration-unstable-v1-server-protocol.h"
#include "xdg-output-unstable-v1-server-protocol.h"
#include "fractional-scale-v1-server-protocol.h"
#include "pointer-constraints-unstable-v1-server-protocol.h"
#include "relative-pointer-unstable-v1-server-protocol.h"
#include "presentation-time-server-protocol.h"
#include "linux-dmabuf-v1-server-protocol.h"
#include "android-wlegl-server-protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <android/log.h>

#define LOG_TAG "TrierarchWayland"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

volatile const char *g_wayland_checkpoint = "init";

struct wayland_server *g_wayland_server;

static struct wl_listener g_client_created_listener;

static void client_created_notify(struct wl_listener *listener, void *data) {
    (void)listener;
    struct wl_client *client = (struct wl_client *)data;
    pid_t pid = (pid_t)-1;
    uid_t uid = 0;
    gid_t gid = 0;
    wl_client_get_credentials(client, &pid, &uid, &gid);
    LOGI("Wayland client connected (pid=%d uid=%u) — expect registry bind + later zwp_linux_dmabuf if GPU client",
            (int)pid, (unsigned)uid);
}

static wayland_server_t *compositor_create_impl(const char *runtime_dir, const char *socket_name) {
    if (!runtime_dir || !socket_name || socket_name[0] == '\0') return NULL;
    struct wayland_server *srv = calloc(1, sizeof(*srv));
    if (!srv) return NULL;
    srv->runtime_dir = strdup(runtime_dir);
    if (!srv->runtime_dir) {
        free(srv);
        return NULL;
    }
    srv->display = wl_display_create();
    if (!srv->display) {
        free(srv->runtime_dir);
        free(srv);
        return NULL;
    }
    pthread_mutex_init(&srv->surfaces_mutex, NULL);
    wl_list_init(&srv->surfaces);
    wl_list_init(&srv->pending_frame_callbacks);
    wl_list_init(&srv->output_resources);
    wl_list_init(&srv->wm_base_resources);
    wl_list_init(&srv->pointer_resources);
    wl_list_init(&srv->relative_pointer_resources);
    wl_list_init(&srv->keyboard_resources);
    wl_list_init(&srv->data_device_resources);
    srv->pointer_focus = NULL;
    srv->keyboard_focus = NULL;
    srv->keyboard_mods_depressed = 0;
    srv->keyboard_mods_locked = 0;
    srv->keyboard_lshift_down = false;
    srv->keyboard_rshift_down = false;
    srv->keyboard_lctrl_down = false;
    srv->keyboard_rctrl_down = false;
    srv->keyboard_lalt_down = false;
    srv->keyboard_ralt_down = false;
    srv->keyboard_lmeta_down = false;
    srv->keyboard_rmeta_down = false;
    srv->keyboard_capslock_down = false;
    srv->keyboard_numlock_down = false;
    srv->keyboard_scrolllock_down = false;
    srv->keyboard_capslock_enabled = false;
    srv->keyboard_numlock_enabled = false;
    srv->keyboard_scrolllock_enabled = false;
    srv->selection_source = NULL;
    srv->cursor_surface = NULL;
    srv->cursor_hotspot_x = srv->cursor_hotspot_y = 0;
    srv->cursor_phys_x = srv->cursor_phys_y = 0.f;
    srv->cursor_visible = true;
    srv->output_width = 1080;
    srv->output_height = 1920;
    srv->output_scale = 1;
    srv->output_user_scale = 1;
    srv->output_override_w = srv->output_override_h = 0;
    wl_list_init(&srv->xdg_output_resources);
    srv->egl_buffer_supported = false;
    srv->next_z_order = 1;
    srv->cascade_x = 0;
    srv->cascade_y = 0;
    srv->wm_mode = WM_MODE_NESTED;  /* default: backward-compatible desktop mode */
    srv->valid = true;

    setenv("XDG_RUNTIME_DIR", runtime_dir, 1);
    chmod(runtime_dir, 0755);

    {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.lock", runtime_dir, socket_name);
        unlink(path);
        snprintf(path, sizeof(path), "%s/%s", runtime_dir, socket_name);
        unlink(path);
    }
    if (wl_display_add_socket(srv->display, socket_name) < 0) {
        LOGE("wl_display_add_socket failed");
        wl_display_destroy(srv->display);
        free(srv->runtime_dir);
        free(srv);
        return NULL;
    }
    /* Default socket mode can be too restrictive for non-root guest users (Plasma as su user). */
    {
        char sockpath[512];
        snprintf(sockpath, sizeof(sockpath), "%s/%s", runtime_dir, socket_name);
        chmod(sockpath, 0666);
    }

    wl_global_create(srv->display, &wl_compositor_interface, 4, srv, surface_compositor_bind);
    wl_global_create(srv->display, &wl_subcompositor_interface, 1, srv, subcompositor_bind);
    wl_global_create(srv->display, &wl_shm_interface, 1, srv, buffer_shm_bind);
    /* v4: zwp_linux_dmabuf_feedback_v1 (required by Mesa 26+ Wayland WSI for presentation checks). */
    wl_global_create(srv->display, &zwp_linux_dmabuf_v1_interface, 4, srv, linux_dmabuf_bind);
    wl_global_create(srv->display, &android_wlegl_interface, 1, srv, android_wlegl_bind);
    LOGI("global advertised: android_wlegl v1 (AHardwareBuffer path; requires client support)");
    wl_global_create(srv->display, &wl_seat_interface, 5, srv, seat_bind);
    wl_global_create(srv->display, &wl_output_interface, 4, srv, output_bind);
    wl_global_create(srv->display, &xdg_wm_base_interface, 4, srv, xdg_shell_bind);
    wl_global_create(srv->display, &wl_data_device_manager_interface, 3, srv, data_device_manager_bind);
    wl_global_create(srv->display, &wp_single_pixel_buffer_manager_v1_interface, 1, srv, single_pixel_buffer_bind);
    wl_global_create(srv->display, &wp_viewporter_interface, 1, srv, viewporter_bind);
    wl_global_create(srv->display, &zxdg_decoration_manager_v1_interface, 1, srv, xdg_decoration_manager_bind);
    wl_global_create(srv->display, &zxdg_output_manager_v1_interface, 3, srv, xdg_output_manager_bind);
    wl_global_create(srv->display, &wp_fractional_scale_manager_v1_interface, 1, srv, fractional_scale_manager_bind);
    wl_global_create(srv->display, &zwp_pointer_constraints_v1_interface, 1, srv, pointer_constraints_bind);
    wl_global_create(srv->display, &zwp_relative_pointer_manager_v1_interface, 1, srv, relative_pointer_manager_bind);
    wl_global_create(srv->display, &wp_presentation_interface, 2, srv, presentation_bind);

    g_client_created_listener.notify = client_created_notify;
    wl_display_add_client_created_listener(srv->display, &g_client_created_listener);

    srv->loop = wl_display_get_event_loop(srv->display);
    g_wayland_server = srv;
    LOGI("Wayland server ready: socket=%s, XDG_RUNTIME_DIR=%s", socket_name, runtime_dir);
    return (wayland_server_t *)srv;
}

wayland_server_t *compositor_create(const char *runtime_dir) {
    return compositor_create_impl(runtime_dir, "wayland-trierarch");
}

wayland_server_t *compositor_create_named(const char *runtime_dir, const char *socket_name) {
    return compositor_create_impl(runtime_dir, socket_name);
}

void compositor_destroy(wayland_server_t *srv_opaque) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    if (g_wayland_server == srv)
        g_wayland_server = NULL;
    pthread_mutex_destroy(&srv->surfaces_mutex);
    if (srv->display) {
        wl_display_destroy(srv->display);
        srv->display = NULL;
    }
    free(srv->runtime_dir);
    srv->valid = false;
    free(srv);
}

void compositor_dispatch(wayland_server_t *srv_opaque) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    wl_display_flush_clients(srv->display);
    wl_event_loop_dispatch(srv->loop, 0);
}

void compositor_dispatch_timeout(wayland_server_t *srv_opaque, int timeout_ms) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    wl_display_flush_clients(srv->display);
    wl_event_loop_dispatch(srv->loop, timeout_ms);
}

int compositor_get_surface_count(wayland_server_t *srv_opaque) {
    if (!srv_opaque) return 0;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    int n = 0;
    struct compositor_surface *surf;
    pthread_mutex_lock(&srv->surfaces_mutex);
    wl_list_for_each(surf, &srv->surfaces, link) {
        if (!surf->parent && surf->current_buffer)
            n++;
    }
    pthread_mutex_unlock(&srv->surfaces_mutex);
    return n;
}

bool compositor_has_toplevel_client(wayland_server_t *srv_opaque) {
    if (!srv_opaque) return false;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    bool has = false;
    struct compositor_surface *surf;
    pthread_mutex_lock(&srv->surfaces_mutex);
    wl_list_for_each(surf, &srv->surfaces, link) {
        if (surf->xdg_toplevel_res && !surf->is_cursor) {
            has = true;
            break;
        }
    }
    pthread_mutex_unlock(&srv->surfaces_mutex);
    return has;
}

static void fill_surface_view(const struct compositor_surface *surf,
        int32_t out_w, int32_t out_h, int32_t scale, compositor_surface_view_t *view) {
    view->pixels = buffer_ref_get_data(surf->current_buffer);
    view->egl_buffer = buffer_ref_get_egl_resource(surf->current_buffer);
    view->dmabuf = buffer_ref_get_dmabuf(surf->current_buffer);
    view->ahb = NULL;
    if (surf->current_buffer && surf->current_buffer->type == BUF_AHB && surf->current_buffer->u.ahb)
        view->ahb = surf->current_buffer->u.ahb->ahb;
    int32_t buf_w = buffer_ref_width(surf->current_buffer);
    int32_t buf_h = buffer_ref_height(surf->current_buffer);
    int32_t s = surf->buffer_scale > 1 ? surf->buffer_scale : 1;
    view->width = buf_w / s;
    view->height = buf_h / s;
    if (view->width <= 0) view->width = buf_w;
    if (view->height <= 0) view->height = buf_h;
    /* Apply wp_viewporter destination size (logical size after scaling). */
    if (surf->viewport_dst_set && surf->viewport_dst_w > 0 && surf->viewport_dst_h > 0) {
        view->width = surf->viewport_dst_w;
        view->height = surf->viewport_dst_h;
    }
    view->stride = buffer_ref_stride(surf->current_buffer);
    view->format = buffer_ref_format(surf->current_buffer);
    view->x = 0;
    view->y = 0;
    view->buf_width = buf_w;
    view->buf_height = buf_h;
    view->src_x = 0.f;
    view->src_y = 0.f;
    view->src_w = 1.f;
    view->src_h = 1.f;
    /* Apply wp_viewporter source rectangle (buffer-space crop). */
    if (surf->viewport_src_set && buf_w > 0 && buf_h > 0) {
        float sx = (float)wl_fixed_to_double(surf->viewport_src_x);
        float sy = (float)wl_fixed_to_double(surf->viewport_src_y);
        float sw = (float)wl_fixed_to_double(surf->viewport_src_w);
        float sh = (float)wl_fixed_to_double(surf->viewport_src_h);
        if (sw > 0.f && sh > 0.f) {
            view->src_x = sx / (float)buf_w;
            view->src_y = sy / (float)buf_h;
            view->src_w = sw / (float)buf_w;
            view->src_h = sh / (float)buf_h;
            if (view->src_x < 0.f) view->src_x = 0.f;
            if (view->src_y < 0.f) view->src_y = 0.f;
            if (view->src_w < 0.f) view->src_w = 0.f;
            if (view->src_h < 0.f) view->src_h = 0.f;
            if (view->src_x + view->src_w > 1.f) view->src_w = 1.f - view->src_x;
            if (view->src_y + view->src_h > 1.f) view->src_h = 1.f - view->src_y;
        }
    }
    view->position_in_physical = false;
}

/* Draw one surface and recurse into children (like ArchMobile foreach_surface_tree). */
static int foreach_surface_tree(struct compositor_surface *surf, int acc_x, int acc_y,
        int32_t out_w, int32_t out_h, int32_t scale,
        int (*callback)(const compositor_surface_view_t *view, void *user), void *user) {
    if (!surf || !callback) return 0;
    if (surf->is_cursor) return 0;  /* cursor surface is drawn separately at pointer position */
    struct compositor_buffer_ref *ref = surf->current_buffer;
    if (ref && buffer_ref_width(ref) > 0 && buffer_ref_height(ref) > 0) {
        void *pixels = buffer_ref_get_data(ref);
        void *egl_buf = buffer_ref_get_egl_resource(ref);
        struct dmabuf_buffer *dma = buffer_ref_get_dmabuf(ref);
        void *ahb = (ref->type == BUF_AHB && ref->u.ahb) ? ref->u.ahb->ahb : NULL;
        if (pixels || egl_buf || dma || ahb) {
            compositor_surface_view_t view;
            fill_surface_view(surf, out_w, out_h, scale, &view);
            view.x = acc_x;
            view.y = acc_y;
            view.position_in_physical = false;
            if (callback(&view, user) != 0) return 1;
        }
    }
    struct compositor_surface *child;
    wl_list_for_each(child, &surf->children, sub_link) {
        int ret = foreach_surface_tree(child, acc_x + child->sub_x, acc_y + child->sub_y,
                out_w, out_h, scale, callback, user);
        if (ret != 0) return ret;
    }
    return 0;
}

/* Logical size of a surface in output coords (preferred: viewport dst > buffer/scale). */
void compositor_surface_get_logical_size(struct compositor_surface *surf, int32_t *w, int32_t *h) {
    *w = 0;
    *h = 0;
    if (!surf || !surf->current_buffer) return;
    if (surf->viewport_dst_set && surf->viewport_dst_w > 0 && surf->viewport_dst_h > 0) {
        *w = surf->viewport_dst_w;
        *h = surf->viewport_dst_h;
        return;
    }
    int32_t bw = buffer_ref_width(surf->current_buffer);
    int32_t bh = buffer_ref_height(surf->current_buffer);
    int32_t s = surf->buffer_scale > 1 ? surf->buffer_scale : 1;
    *w = bw / s;
    *h = bh / s;
    if (*w <= 0) *w = bw;
    if (*h <= 0) *h = bh;
}

/* Bump surf to top of stacking order. */
void compositor_raise_surface(struct wayland_server *srv, struct compositor_surface *surf) {
    if (!surf || !srv) return;
    surf->z_order = srv->next_z_order++;
}

/* KDE stacks a 1x1 (or tiny) buffer + wp_viewport destination above the real desktop if we iterate
 * in wl_list order. That pixel is often black → wrong region or full screen black. Draw
 * viewporter-upscaled tiny buffers first (underlay), then normal surfaces. */
static int32_t g_sort_out_w, g_sort_out_h;

static int surface_is_viewporter_underlay(struct compositor_surface *s, int32_t out_w, int32_t out_h) {
    if (!s || !s->current_buffer) return 0;
    int32_t bw = buffer_ref_width(s->current_buffer);
    int32_t bh = buffer_ref_height(s->current_buffer);
    if (bw <= 0 || bh <= 0) return 0;
    if (!s->viewport_dst_set || s->viewport_dst_w <= 0 || s->viewport_dst_h <= 0) return 0;
    int64_t buf_a = (int64_t)bw * (int64_t)bh;
    int64_t out_a = (int64_t)out_w * (int64_t)out_h;
    if (out_a <= 0) return 0;
    /* Tiny buffer + viewporter upscale: Plasma may use 1x1 with a destination that is only part
     * of the output (e.g. fractional scale → vp_dst is ~1/4 of the output). The old 75%-of-output
     * rule missed that case, so the 1x1 stayed in arbitrary z-order and could paint black on top. */
    if (buf_a <= 256)
        return 1;
    int64_t dst_a = (int64_t)s->viewport_dst_w * (int64_t)s->viewport_dst_h;
    return dst_a * 100 >= out_a * 75; /* larger tile: treat as underlay only if ~fullscreen dest */
}

/* Primary sort: ascending z_order (lower drawn first = appears below).
 * Secondary: underlay-first for tied z_order (backward compat for nested KDE). */
static int cmp_root_surface_zorder(const void *aa, const void *bb) {
    struct compositor_surface *const *a = aa;
    struct compositor_surface *const *b = bb;
    if ((*a)->z_order != (*b)->z_order)
        return ((*a)->z_order < (*b)->z_order) ? -1 : 1;
    int ua = surface_is_viewporter_underlay(*a, g_sort_out_w, g_sort_out_h);
    int ub = surface_is_viewporter_underlay(*b, g_sort_out_w, g_sort_out_h);
    if (ua != ub) return ua ? -1 : 1;
    return 0;
}

void compositor_foreach_surface(wayland_server_t *srv_opaque,
        int (*callback)(const compositor_surface_view_t *view, void *user), void *user) {
    if (!srv_opaque || !callback) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    int32_t out_w = srv->output_width > 0 ? srv->output_width : 1;
    int32_t out_h = srv->output_height > 0 ? srv->output_height : 1;
    int32_t scale = srv->output_scale > 0 ? srv->output_scale : 1;

    pthread_mutex_lock(&srv->surfaces_mutex);
    int n = 0;
    struct compositor_surface *s;
    wl_list_for_each(s, &srv->surfaces, link) {
        if (!s->parent && !s->is_cursor) n++;
    }
    struct compositor_surface **roots = NULL;
    if (n > 0) {
        roots = (struct compositor_surface **)calloc((size_t)n, sizeof(*roots));
        if (!roots) {
            pthread_mutex_unlock(&srv->surfaces_mutex);
            return;
        }
        int i = 0;
        wl_list_for_each(s, &srv->surfaces, link) {
            if (s->parent || s->is_cursor) continue;
            roots[i++] = s;
        }
        g_sort_out_w = out_w;
        g_sort_out_h = out_h;
        if (srv->wm_mode == WM_MODE_DIRECT) {
            /* Direct mode: sort by z_order (ascending = lower drawn first = visually below).
             * Secondary: underlay-first for tied z_order. */
            qsort(roots, (size_t)n, sizeof(*roots), cmp_root_surface_zorder);
            for (i = 0; i < n; i++) {
                if (foreach_surface_tree(roots[i], roots[i]->wm_x, roots[i]->wm_y,
                            out_w, out_h, scale, callback, user) != 0)
                    break;
            }
        } else {
            /* Nested mode: draw all surfaces at (0,0); use underlay-first sort only
             * (KDE Plasma 1x1 viewport hack). */
            qsort(roots, (size_t)n, sizeof(*roots), cmp_root_surface_zorder);
            for (i = 0; i < n; i++) {
                if (foreach_surface_tree(roots[i], 0, 0,
                            out_w, out_h, scale, callback, user) != 0)
                    break;
            }
        }
        free(roots);
    }
    pthread_mutex_unlock(&srv->surfaces_mutex);
}

void compositor_set_cursor_physical(wayland_server_t *srv_opaque, float x, float y) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    pthread_mutex_lock(&srv->surfaces_mutex);
    srv->cursor_phys_x = x;
    srv->cursor_phys_y = y;
    pthread_mutex_unlock(&srv->surfaces_mutex);
}

void compositor_set_cursor_visible(wayland_server_t *srv_opaque, bool visible) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    srv->cursor_visible = visible;
}

bool compositor_get_cursor_view(wayland_server_t *srv_opaque, compositor_surface_view_t *out) {
    if (!srv_opaque || !out) return false;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    if (!srv->cursor_visible) return false;
    struct compositor_surface *cs;
    int32_t hx, hy;
    float cx, cy;
    pthread_mutex_lock(&srv->surfaces_mutex);
    cs = srv->cursor_surface;
    hx = srv->cursor_hotspot_x;
    hy = srv->cursor_hotspot_y;
    cx = srv->cursor_phys_x;
    cy = srv->cursor_phys_y;
    pthread_mutex_unlock(&srv->surfaces_mutex);
    if (!cs || !cs->current_buffer) return false;
    int32_t buf_w = buffer_ref_width(cs->current_buffer);
    int32_t buf_h = buffer_ref_height(cs->current_buffer);
    if (buf_w <= 0 || buf_h <= 0) return false;
    out->pixels = buffer_ref_get_data(cs->current_buffer);
    out->egl_buffer = buffer_ref_get_egl_resource(cs->current_buffer);
    out->dmabuf = buffer_ref_get_dmabuf(cs->current_buffer);
    if (!out->pixels && !out->egl_buffer && !out->dmabuf) return false;
    /* Keep cursor size stable in physical pixels across output scale changes:
     * clients may choose a larger cursor buffer when scale increases. We intentionally ignore
     * output scaling here and draw a fixed-size cursor in physical pixels. */
    const int32_t target_px = 80; /* tweakable: visual cursor size in physical pixels */
    int32_t draw_w = target_px;
    int32_t draw_h = target_px;
    if (buf_w > 0 && buf_h > 0) {
        /* Preserve aspect ratio for non-square cursor buffers. */
        if (buf_w >= buf_h) {
            draw_h = (int32_t)((int64_t)target_px * buf_h / buf_w);
        } else {
            draw_w = (int32_t)((int64_t)target_px * buf_w / buf_h);
        }
    }
    if (draw_w < 16) draw_w = 16;
    if (draw_h < 16) draw_h = 16;
    out->width = draw_w;
    out->height = draw_h;
    out->stride = buffer_ref_stride(cs->current_buffer);
    out->format = buffer_ref_format(cs->current_buffer);
    out->buf_width = buf_w;
    out->buf_height = buf_h;
    out->src_x = 0.f;
    out->src_y = 0.f;
    out->src_w = 1.f;
    out->src_h = 1.f;
    /* Scale hotspot with the same factor as the drawn cursor size to avoid "jump". */
    float sx = (buf_w > 0) ? ((float)draw_w / (float)buf_w) : 1.0f;
    float sy = (buf_h > 0) ? ((float)draw_h / (float)buf_h) : 1.0f;
    int32_t hx_draw = (int32_t)((float)hx * sx + 0.5f);
    int32_t hy_draw = (int32_t)((float)hy * sy + 0.5f);
    out->x = (int32_t)(cx - (float)hx_draw);
    out->y = (int32_t)(cy - (float)hy_draw);
    out->position_in_physical = true;
    return true;
}

void compositor_send_frame_callbacks(wayland_server_t *srv_opaque) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t now_ms = (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    struct pending_frame_cb *p, *tmp;
    wl_list_for_each_safe(p, tmp, &srv->pending_frame_callbacks, link) {
        wl_callback_send_done(p->resource, now_ms);
        wl_list_remove(&p->link);
        wl_list_remove(&p->resource_listener.link);
        wl_resource_destroy(p->resource);
        free(p);
    }
}

#define PING_INTERVAL_MS 1000
void compositor_send_ping_to_clients(wayland_server_t *srv_opaque) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    static struct timespec last_ping;
    static int last_init;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (!last_init) {
        last_ping = now;
        last_init = 1;
    }
    long elapsed_ms = (long)(now.tv_sec - last_ping.tv_sec) * 1000 +
                     (long)(now.tv_nsec - last_ping.tv_nsec) / 1000000;
    if (elapsed_ms < PING_INTERVAL_MS) return;
    last_ping = now;

    struct wm_base_resource_node *node;
    wl_list_for_each(node, &srv->wm_base_resources, link) {
        if (node->resource && wl_resource_get_version(node->resource) >= 1) {
            uint32_t serial = wl_display_next_serial(srv->display);
            xdg_wm_base_send_ping(node->resource, serial);
        }
    }
}

void *compositor_get_wl_display(wayland_server_t *srv_opaque) {
    if (!srv_opaque) return NULL;
    return (void *)((struct wayland_server *)srv_opaque)->display;
}

void compositor_set_egl_buffer_supported(wayland_server_t *srv_opaque, bool supported) {
    if (!srv_opaque) return;
    ((struct wayland_server *)srv_opaque)->egl_buffer_supported = supported;
}

void compositor_set_wm_mode(wayland_server_t *srv_opaque, wm_mode_t mode) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    srv->wm_mode = mode;
    /* Reset transient WM/input state so switching modes doesn't leak behaviour across modes. */
    pthread_mutex_lock(&srv->surfaces_mutex);
    srv->wm_drag_surf = NULL;
    srv->wm_resize_surf = NULL;
    srv->wm_last_click_surf = NULL;
    srv->wm_last_click_time_ms = 0;
    srv->pointer_focus = NULL;
    srv->keyboard_focus = NULL;
    pthread_mutex_unlock(&srv->surfaces_mutex);
    LOGI("wm_mode set to %s", mode == WM_MODE_DIRECT ? "DIRECT" : "NESTED");
}
