/*
 * Pointer input: track wl_pointer/zwp_relative_pointer_v1 resources, find target surface,
 * and deliver touch as wl_pointer (absolute coords) and zwp_relative_pointer_v1 (delta).
 * App maps: relative input (touchpad-style) and absolute input (tablet-style) to wl_pointer + relative_pointer.
 */
#include "server_internal.h"
#include "relative-pointer-unstable-v1-server-protocol.h"
#include <wayland-util.h>
#include <stdlib.h>
#include <string.h>

#define POINTER_ACTION_DOWN       0
#define POINTER_ACTION_MOVE       1
#define POINTER_ACTION_UP         2
/* 6 = hover move (absolute cursor position), used when app is in relative-input mode */
#define POINTER_ACTION_POINTER_MOVE 6

#define BTN_LEFT  0x110
#define BTN_RIGHT 0x111
#define WL_POINTER_BUTTON_STATE_PRESSED  1
#define WL_POINTER_BUTTON_STATE_RELEASED 0

/*
 * Input model contract:
 * - This compositor uses a "best fullscreen surface" heuristic for pointer focus because we target
 *   a single-desktop session (Plasma/KWin) rather than multi-window stacking on Android.
 * - In touchpad (relative) mode, the app reports absolute cursor coordinates via POINTER_MOVE (6),
 *   and we also emit relative motion deltas for clients that use zwp_relative_pointer_v1.
 * - In tablet (absolute) mode, the app reports touch coordinates directly as wl_pointer positions.
 */

static void input_resource_node_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct input_resource_node *node = wl_container_of(listener, node, destroy_listener);
    wl_list_remove(&node->link);
    wl_list_remove(&node->destroy_listener.link);
    free(node);
}

void track_input_resource(struct wl_list *list, struct wl_resource *res) {
    struct input_resource_node *node = calloc(1, sizeof(*node));
    if (!node) return;
    node->resource = res;
    node->destroy_listener.notify = input_resource_node_destroy;
    wl_resource_add_destroy_listener(res, &node->destroy_listener);
    wl_list_insert(list, &node->link);
}

/* Pick the best toplevel surface for pointer focus (single fullscreen window). */
static struct compositor_surface *find_pointer_target(struct wayland_server *srv) {
    struct compositor_surface *best = NULL;
    int64_t best_score = 0;
    struct compositor_surface *surf;
    wl_list_for_each(surf, &srv->surfaces, link) {
        if (surf->parent) continue;
        if (surf->is_cursor) continue;  /* do not target cursor surface for pointer events */
        int64_t score = 0;
        if (surf->current_buffer) {
            int32_t w = buffer_ref_width(surf->current_buffer);
            int32_t h = buffer_ref_height(surf->current_buffer);
            score = (int64_t)w * h;
        }
        if (surf->xdg_toplevel_res) score += 100000000LL;
        if (score < 128 * 128 && wl_list_empty(&surf->children) && !surf->xdg_toplevel_res)
            continue;
        if (score > best_score) {
            best_score = score;
            best = surf;
        }
    }
    return best;
}

static void pointer_send_enter(struct wayland_server *srv,
        struct compositor_surface *surf, wl_fixed_t x, wl_fixed_t y) {
    if (!surf || !surf->resource) return;
    struct wl_client *client = wl_resource_get_client(surf->resource);
    uint32_t serial = wl_display_next_serial(srv->display);
    struct input_resource_node *node;
    wl_list_for_each(node, &srv->pointer_resources, link) {
        if (wl_resource_get_client(node->resource) == client) {
            wl_pointer_send_enter(node->resource, serial, surf->resource, x, y);
            if (wl_resource_get_version(node->resource) >= 5)
                wl_pointer_send_frame(node->resource);
        }
    }
}

static void pointer_send_leave(struct wayland_server *srv,
        struct compositor_surface *surf) {
    if (!surf || !surf->resource) return;
    struct wl_client *client = wl_resource_get_client(surf->resource);
    uint32_t serial = wl_display_next_serial(srv->display);
    struct input_resource_node *node;
    wl_list_for_each(node, &srv->pointer_resources, link) {
        if (wl_resource_get_client(node->resource) == client) {
            wl_pointer_send_leave(node->resource, serial, surf->resource);
            if (wl_resource_get_version(node->resource) >= 5)
                wl_pointer_send_frame(node->resource);
        }
    }
}

static void pointer_send_motion(struct wayland_server *srv,
        struct compositor_surface *surf, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    if (!surf || !surf->resource) return;
    struct wl_client *client = wl_resource_get_client(surf->resource);
    struct input_resource_node *node;
    wl_list_for_each(node, &srv->pointer_resources, link) {
        if (wl_resource_get_client(node->resource) == client) {
            wl_pointer_send_motion(node->resource, time, x, y);
            if (wl_resource_get_version(node->resource) >= 5)
                wl_pointer_send_frame(node->resource);
        }
    }
}

static void pointer_send_button(struct wayland_server *srv,
        struct compositor_surface *surf, uint32_t time, uint32_t button, uint32_t state) {
    if (!surf || !surf->resource) return;
    struct wl_client *client = wl_resource_get_client(surf->resource);
    uint32_t serial = wl_display_next_serial(srv->display);
    struct input_resource_node *node;
    wl_list_for_each(node, &srv->pointer_resources, link) {
        if (wl_resource_get_client(node->resource) == client) {
            wl_pointer_send_button(node->resource, serial, time, button, state);
            if (wl_resource_get_version(node->resource) >= 5)
                wl_pointer_send_frame(node->resource);
        }
    }
}

static void send_relative_motion(struct wayland_server *srv,
        struct compositor_surface *surf, uint32_t time_ms,
        wl_fixed_t dx, wl_fixed_t dy) {
    if (!surf || !surf->resource) return;
    struct wl_client *client = wl_resource_get_client(surf->resource);
    uint64_t us = (uint64_t)time_ms * 1000u;
    uint32_t utime_hi = (uint32_t)(us >> 32);
    uint32_t utime_lo = (uint32_t)(us & 0xFFFFFFFFu);
    struct input_resource_node *node;
    wl_list_for_each(node, &srv->relative_pointer_resources, link) {
        if (wl_resource_get_client(node->resource) == client) {
            zwp_relative_pointer_v1_send_relative_motion(node->resource,
                    utime_hi, utime_lo, dx, dy, dx, dy);
        }
    }
}

/* Send pointer axis (scroll) to focused surface. One axis_source and one frame per client. */
static void pointer_send_axis(struct wayland_server *srv,
        struct compositor_surface *surf, uint32_t time_ms,
        wl_fixed_t value_v, wl_fixed_t value_h, uint32_t axis_source) {
    if (!surf || !surf->resource) return;
    struct wl_client *client = wl_resource_get_client(surf->resource);
    struct input_resource_node *node;
    wl_list_for_each(node, &srv->pointer_resources, link) {
        if (wl_resource_get_client(node->resource) != client) continue;
        struct wl_resource *res = node->resource;
        uint32_t ver = wl_resource_get_version(res);
        if (ver >= 5)
            wl_pointer_send_axis_source(res, axis_source);
        if (value_v != 0) {
            wl_pointer_send_axis(res, time_ms, WL_POINTER_AXIS_VERTICAL_SCROLL, value_v);
        }
        if (value_h != 0) {
            wl_pointer_send_axis(res, time_ms, WL_POINTER_AXIS_HORIZONTAL_SCROLL, value_h);
        }
        if (ver >= 5)
            wl_pointer_send_frame(res);
    }
}

void compositor_pointer_axis_event(wayland_server_t *srv_opaque, uint32_t time_ms,
        float delta_x, float delta_y, uint32_t axis_source) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    /*
     * JNI deltas: Kotlin sends two-finger centroid deltas (dx, dy) in surface pixels, or
     * (-AXIS_HSCROLL, -AXIS_VSCROLL) for a physical wheel. Map to wl_pointer axis values:
     * vertical positive = scroll down, horizontal positive = scroll right.
     * Finger centroid dy follows Android surface Y (down positive); wheel uses MotionEvent axes.
     * Horizontal matches after negating dx; vertical matches wheel only if finger dy is negated here.
     */
    float dy = delta_y;
    if (axis_source == WL_POINTER_AXIS_SOURCE_FINGER)
        dy = -dy;
    wl_fixed_t vy = wl_fixed_from_double((double)dy);
    wl_fixed_t vx = wl_fixed_from_double((double)(-delta_x));
    if (vy == 0 && vx == 0) return;
    pthread_mutex_lock(&srv->surfaces_mutex);
    struct compositor_surface *surf = srv->pointer_focus;
    if (surf && surf->resource)
        pointer_send_axis(srv, surf, time_ms, vy, vx, axis_source);
    pthread_mutex_unlock(&srv->surfaces_mutex);
}

void compositor_pointer_right_click(wayland_server_t *srv_opaque, uint32_t time_ms, float x, float y) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    wl_fixed_t fx = wl_fixed_from_double((double)x);
    wl_fixed_t fy = wl_fixed_from_double((double)y);
    pthread_mutex_lock(&srv->surfaces_mutex);
    srv->pointer_x = fx;
    srv->pointer_y = fy;
    /* do not overwrite cursor_phys: app already set it via nativeSetCursorPhysical for drawing */
    struct compositor_surface *surf = srv->pointer_focus;
    if (!surf || !surf->resource) {
        surf = find_pointer_target(srv);
        if (surf && surf->resource) {
            srv->pointer_focus = surf;
            pointer_send_enter(srv, surf, fx, fy);
            keyboard_focus_update(srv, surf);
        }
    }
    if (surf && surf->resource) {
        pointer_send_motion(srv, surf, time_ms, fx, fy);
        pointer_send_button(srv, surf, time_ms, BTN_RIGHT, WL_POINTER_BUTTON_STATE_PRESSED);
        pointer_send_button(srv, surf, time_ms, BTN_RIGHT, WL_POINTER_BUTTON_STATE_RELEASED);
    }
    pthread_mutex_unlock(&srv->surfaces_mutex);
}

void compositor_pointer_event(wayland_server_t *srv_opaque, float x, float y, int action, uint32_t time_ms) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    wl_fixed_t fx = wl_fixed_from_double((double)x);
    wl_fixed_t fy = wl_fixed_from_double((double)y);

    pthread_mutex_lock(&srv->surfaces_mutex);

    struct compositor_surface *target = find_pointer_target(srv);

    switch (action) {
    case POINTER_ACTION_POINTER_MOVE: {
        /* Hover move: x,y are absolute cursor position (relative-input mode from app). */
        wl_fixed_t old_px = srv->pointer_x, old_py = srv->pointer_y;
        srv->pointer_x = fx;
        srv->pointer_y = fy;
        if (srv->pointer_focus != target) {
            if (srv->pointer_focus)
                pointer_send_leave(srv, srv->pointer_focus);
            srv->pointer_focus = target;
            if (target)
                pointer_send_enter(srv, target, fx, fy);
            keyboard_focus_update(srv, target);
        }
        if (target && target->resource) {
            pointer_send_motion(srv, target, time_ms, fx, fy);
            send_relative_motion(srv, target, time_ms, fx - old_px, fy - old_py);
        }
        break;
    }
    case POINTER_ACTION_DOWN:
        srv->pointer_x = fx;
        srv->pointer_y = fy;
        if (srv->pointer_focus != target) {
            if (srv->pointer_focus)
                pointer_send_leave(srv, srv->pointer_focus);
            srv->pointer_focus = target;
            if (target)
                pointer_send_enter(srv, target, fx, fy);
            keyboard_focus_update(srv, target);
        }
        if (target) {
            pointer_send_motion(srv, target, time_ms, fx, fy);
            pointer_send_button(srv, target, time_ms, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
        }
        break;
    case POINTER_ACTION_MOVE: {
        wl_fixed_t old_x = srv->pointer_x, old_y = srv->pointer_y;
        srv->pointer_x = fx;
        srv->pointer_y = fy;
        if (srv->pointer_focus && srv->pointer_focus->resource) {
            pointer_send_motion(srv, srv->pointer_focus, time_ms, fx, fy);
            send_relative_motion(srv, srv->pointer_focus, time_ms, fx - old_x, fy - old_y);
        }
        break;
    }
    case POINTER_ACTION_UP:
        srv->pointer_x = fx;
        srv->pointer_y = fy;
        if (srv->pointer_focus && srv->pointer_focus->resource)
            pointer_send_button(srv, srv->pointer_focus, time_ms, BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
        break;
    default:
        break;
    }

    pthread_mutex_unlock(&srv->surfaces_mutex);
}
