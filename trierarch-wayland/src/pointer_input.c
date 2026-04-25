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

#define WM_TITLEBAR_HOT_Y 36
#define WM_RESIZE_HOT_PX  24
#define WM_MIN_W          96
#define WM_MIN_H          64

#define WM_EDGE_LEFT   1u
#define WM_EDGE_RIGHT  2u
#define WM_EDGE_TOP    4u
#define WM_EDGE_BOTTOM 8u

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

/* Pick the best surface to receive pointer focus.
 *
 * NESTED mode: largest-area heuristic (single fullscreen client, no position tracking).
 * DIRECT mode: hit-test by position + z_order; fallback to topmost if pointer is in a gap.
 */
static struct compositor_surface *find_pointer_target(struct wayland_server *srv,
        wl_fixed_t fx, wl_fixed_t fy) {
    struct compositor_surface *surf;

    if (srv->wm_mode == WM_MODE_DIRECT) {
        float px = (float)wl_fixed_to_double(fx);
        float py = (float)wl_fixed_to_double(fy);

        /* Pass 1: hit-test toplevels, pick the one with the highest z_order under the cursor. */
        struct compositor_surface *hit = NULL;
        int32_t hit_z = -1;
        wl_list_for_each(surf, &srv->surfaces, link) {
            if (surf->parent || surf->is_cursor) continue;
            if (!surf->xdg_toplevel_res || !surf->current_buffer) continue;
            int32_t sw = 0, sh = 0;
            compositor_surface_get_logical_size(surf, &sw, &sh);
            if (sw <= 0 || sh <= 0) continue;
            float x0 = (float)surf->wm_x;
            float y0 = (float)surf->wm_y;
            if (px >= x0 && px < x0 + (float)sw && py >= y0 && py < y0 + (float)sh) {
                if (surf->z_order > hit_z) {
                    hit_z = surf->z_order;
                    hit = surf;
                }
            }
        }
        if (hit) return hit;

        /* Pass 2: pointer is in a gap — return the topmost toplevel regardless of position. */
        struct compositor_surface *top = NULL;
        int32_t top_z = -1;
        wl_list_for_each(surf, &srv->surfaces, link) {
            if (surf->parent || surf->is_cursor || !surf->xdg_toplevel_res) continue;
            if (surf->z_order > top_z) { top_z = surf->z_order; top = surf; }
        }
        return top;
    }

    /* NESTED mode: largest area + xdg_toplevel bonus (original behaviour). */
    struct compositor_surface *best = NULL;
    int64_t best_score = 0;
    wl_list_for_each(surf, &srv->surfaces, link) {
        if (surf->parent || surf->is_cursor) continue;
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

static inline void surface_local_coords(struct wayland_server *srv, struct compositor_surface *surf,
        wl_fixed_t in_x, wl_fixed_t in_y, wl_fixed_t *out_x, wl_fixed_t *out_y) {
    if (!out_x || !out_y) return;
    if (srv && srv->wm_mode == WM_MODE_DIRECT && surf && surf->xdg_toplevel_res) {
        /* wl_pointer coordinates are surface-local; translate from output coords. */
        double dx = wl_fixed_to_double(in_x) - (double)surf->wm_x;
        double dy = wl_fixed_to_double(in_y) - (double)surf->wm_y;
        *out_x = wl_fixed_from_double(dx);
        *out_y = wl_fixed_from_double(dy);
        return;
    }
    *out_x = in_x;
    *out_y = in_y;
}

static inline int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void wm_apply_move_clamped(struct wayland_server *srv, struct compositor_surface *surf,
        int32_t new_x, int32_t new_y) {
    if (!srv || !surf) return;
    int32_t sw = 0, sh = 0;
    compositor_surface_get_logical_size(surf, &sw, &sh);
    if (sw <= 0 || sh <= 0 || srv->output_width <= 0 || srv->output_height <= 0) {
        surf->wm_x = new_x;
        surf->wm_y = new_y;
        return;
    }
    /* Keep at least some part of the window visible. */
    int32_t max_x = srv->output_width - 1;
    int32_t max_y = srv->output_height - 1;
    int32_t min_x = -(sw - 32);
    int32_t min_y = -(WM_TITLEBAR_HOT_Y - 8);
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    surf->wm_x = clamp_i32(new_x, min_x, max_x);
    surf->wm_y = clamp_i32(new_y, min_y, max_y);
}

static void wm_apply_resize_clamped(struct wayland_server *srv, struct compositor_surface *surf,
        int32_t new_x, int32_t new_y, int32_t new_w, int32_t new_h) {
    if (!srv || !surf) return;
    if (new_w < WM_MIN_W) new_w = WM_MIN_W;
    if (new_h < WM_MIN_H) new_h = WM_MIN_H;

    /* Clamp size to output bounds if known. */
    if (srv->output_width > 0) {
        if (new_x < 0) { new_w += new_x; new_x = 0; }
        int32_t maxw = srv->output_width - new_x;
        if (maxw > 0 && new_w > maxw) new_w = maxw;
    }
    if (srv->output_height > 0) {
        if (new_y < 0) { new_h += new_y; new_y = 0; }
        int32_t maxh = srv->output_height - new_y;
        if (maxh > 0 && new_h > maxh) new_h = maxh;
    }
    if (new_w < WM_MIN_W) new_w = WM_MIN_W;
    if (new_h < WM_MIN_H) new_h = WM_MIN_H;

    surf->wm_x = new_x;
    surf->wm_y = new_y;
    surf->wm_req_w = new_w;
    surf->wm_req_h = new_h;
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
    /* JNI: centroid (dx,dy) in pixels for finger, or (-HSCROLL,-VSCROLL) for wheel; +vy/+vx = scroll down/right. */
    float dy = delta_y;
    if (axis_source == WL_POINTER_AXIS_SOURCE_FINGER)
        dy = -dy;
    float dx = -(float)delta_x;

    const float wheel_axis_scale = 8.5f;  /* wheel axis values are often small per detent */
    const float finger_axis_scale = 0.06f; /* finger deltas are full pixels, not line units */

    if (axis_source == WL_POINTER_AXIS_SOURCE_FINGER) {
        dx *= finger_axis_scale;
        dy *= finger_axis_scale;
    } else {
        dx *= wheel_axis_scale;
        dy *= wheel_axis_scale;
    }

    wl_fixed_t vy = wl_fixed_from_double((double)dy);
    wl_fixed_t vx = wl_fixed_from_double((double)dx);
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
    struct compositor_surface *surf = find_pointer_target(srv, fx, fy);
    if (surf && surf != srv->pointer_focus) {
        if (srv->pointer_focus) pointer_send_leave(srv, srv->pointer_focus);
        srv->pointer_focus = surf;
        pointer_send_enter(srv, surf, fx, fy);
        keyboard_focus_update(srv, surf);
        compositor_raise_surface(srv, surf);
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

    switch (action) {
    case POINTER_ACTION_POINTER_MOVE: {
        /* Hover move: x,y are absolute cursor position (relative-input mode from app). */
        struct compositor_surface *target = find_pointer_target(srv, fx, fy);
        wl_fixed_t old_px = srv->pointer_x, old_py = srv->pointer_y;
        srv->pointer_x = fx;
        srv->pointer_y = fy;

        /* If an interactive resize is active, update requested size (direct mode). */
        if (srv->wm_resize_surf) {
            float px = (float)wl_fixed_to_double(fx);
            float py = (float)wl_fixed_to_double(fy);
            int32_t dx = (int32_t)(px - srv->wm_resize_ptr_start_x);
            int32_t dy = (int32_t)(py - srv->wm_resize_ptr_start_y);
            int32_t nx = srv->wm_resize_start_x;
            int32_t ny = srv->wm_resize_start_y;
            int32_t nw = srv->wm_resize_start_w;
            int32_t nh = srv->wm_resize_start_h;

            if (srv->wm_resize_edges & WM_EDGE_LEFT) { nx += dx; nw -= dx; }
            if (srv->wm_resize_edges & WM_EDGE_RIGHT) { nw += dx; }
            if (srv->wm_resize_edges & WM_EDGE_TOP) { ny += dy; nh -= dy; }
            if (srv->wm_resize_edges & WM_EDGE_BOTTOM) { nh += dy; }

            wm_apply_resize_clamped(srv, srv->wm_resize_surf, nx, ny, nw, nh);
            send_toplevel_configure(srv->wm_resize_surf);
            break;
        }
        /* If an interactive move is active, update the window position (direct mode). */
        if (srv->wm_drag_surf) {
            float px = (float)wl_fixed_to_double(fx);
            float py = (float)wl_fixed_to_double(fy);
            int32_t nx = srv->wm_drag_surf_start_x + (int32_t)(px - srv->wm_drag_ptr_start_x);
            int32_t ny = srv->wm_drag_surf_start_y + (int32_t)(py - srv->wm_drag_ptr_start_y);
            wm_apply_move_clamped(srv, srv->wm_drag_surf, nx, ny);
            break;
        }

        if (srv->pointer_focus != target) {
            if (srv->pointer_focus)
                pointer_send_leave(srv, srv->pointer_focus);
            srv->pointer_focus = target;
            if (target) {
                wl_fixed_t lx = fx, ly = fy;
                surface_local_coords(srv, target, fx, fy, &lx, &ly);
                pointer_send_enter(srv, target, lx, ly);
            }
            keyboard_focus_update(srv, target);
        }
        if (target && target->resource) {
            wl_fixed_t lx = fx, ly = fy;
            surface_local_coords(srv, target, fx, fy, &lx, &ly);
            pointer_send_motion(srv, target, time_ms, lx, ly);
            send_relative_motion(srv, target, time_ms, fx - old_px, fy - old_py);
        }
        break;
    }
    case POINTER_ACTION_DOWN: {
        struct compositor_surface *target = find_pointer_target(srv, fx, fy);
        srv->pointer_x = fx;
        srv->pointer_y = fy;
        if (srv->pointer_focus != target) {
            if (srv->pointer_focus)
                pointer_send_leave(srv, srv->pointer_focus);
            srv->pointer_focus = target;
            if (target) {
                wl_fixed_t lx = fx, ly = fy;
                surface_local_coords(srv, target, fx, fy, &lx, &ly);
                pointer_send_enter(srv, target, lx, ly);
            }
            keyboard_focus_update(srv, target);
        }
        /* Raise the clicked window to the top of the stacking order. */
        if (target) {
            compositor_raise_surface(srv, target);
            wl_fixed_t lx = fx, ly = fy;
            surface_local_coords(srv, target, fx, fy, &lx, &ly);

            /* Direct mode: edge/corner resize hotspots (24px). */
            if (srv->wm_mode == WM_MODE_DIRECT && target->xdg_toplevel_res && !srv->wm_resize_surf) {
                int32_t sw = 0, sh = 0;
                compositor_surface_get_logical_size(target, &sw, &sh);
                int32_t local_x = (int32_t)wl_fixed_to_int(lx);
                int32_t local_y = (int32_t)wl_fixed_to_int(ly);
                uint32_t edges = 0;
                if (sw > 0 && sh > 0) {
                    if (local_x >= 0 && local_x < WM_RESIZE_HOT_PX) edges |= WM_EDGE_LEFT;
                    if (local_x >= (sw - WM_RESIZE_HOT_PX) && local_x < sw) edges |= WM_EDGE_RIGHT;
                    if (local_y >= 0 && local_y < WM_RESIZE_HOT_PX) edges |= WM_EDGE_TOP;
                    if (local_y >= (sh - WM_RESIZE_HOT_PX) && local_y < sh) edges |= WM_EDGE_BOTTOM;

                    /* Avoid stealing from titlebar gestures except for top corners. */
                    bool in_top_bar = (local_y >= 0 && local_y < WM_TITLEBAR_HOT_Y);
                    bool top_corner = (local_y < WM_RESIZE_HOT_PX) &&
                        ((local_x < WM_RESIZE_HOT_PX) || (local_x >= sw - WM_RESIZE_HOT_PX));
                    if (in_top_bar && !top_corner) {
                        edges &= ~WM_EDGE_TOP;
                    }
                }

                if (edges != 0) {
                    /* If maximized, auto-exit maximize first, then start resize from restored size. */
                    if (target->wm_maximized) {
                        target->wm_x = target->wm_saved_x;
                        target->wm_y = target->wm_saved_y;
                        target->wm_maximized = false;
                        if (target->wm_saved_w > 0) target->wm_req_w = target->wm_saved_w;
                        if (target->wm_saved_h > 0) target->wm_req_h = target->wm_saved_h;
                        send_toplevel_configure(target);
                    }
                    srv->wm_resize_surf = target;
                    target->wm_resizing = true;
                    srv->wm_resize_ptr_start_x = (float)wl_fixed_to_double(fx);
                    srv->wm_resize_ptr_start_y = (float)wl_fixed_to_double(fy);
                    srv->wm_resize_start_x = target->wm_x;
                    srv->wm_resize_start_y = target->wm_y;
                    srv->wm_resize_start_w = (target->wm_req_w > 0) ? target->wm_req_w : sw;
                    srv->wm_resize_start_h = (target->wm_req_h > 0) ? target->wm_req_h : sh;
                    if (srv->wm_resize_start_w < WM_MIN_W) srv->wm_resize_start_w = WM_MIN_W;
                    if (srv->wm_resize_start_h < WM_MIN_H) srv->wm_resize_start_h = WM_MIN_H;
                    srv->wm_resize_edges = edges;
                }
            }

            /* Direct mode: allow compositor-driven drag when pressing the top strip
             * (acts as a simple titlebar). This avoids relying on clients calling
             * xdg_toplevel.move(), which many apps won't do. */
            if (srv->wm_mode == WM_MODE_DIRECT && !srv->wm_drag_surf && target->xdg_toplevel_res) {
                int32_t local_y = (int32_t)wl_fixed_to_int(ly);
                if (local_y >= 0 && local_y < WM_TITLEBAR_HOT_Y) {
                    /* Double-click titlebar toggles maximize/restore. */
                    uint32_t dt = time_ms - srv->wm_last_click_time_ms;
                    bool is_double =
                        (srv->wm_last_click_surf == target) &&
                        (dt > 0 && dt <= 350u);
                    srv->wm_last_click_time_ms = time_ms;
                    srv->wm_last_click_surf = target;

                    if (is_double) {
                        /* Toggle maximized state and reconfigure. */
                        if (target->wm_maximized) {
                            target->wm_x = target->wm_saved_x;
                            target->wm_y = target->wm_saved_y;
                            target->wm_maximized = false;
                            if (target->wm_saved_w > 0) target->wm_req_w = target->wm_saved_w;
                            if (target->wm_saved_h > 0) target->wm_req_h = target->wm_saved_h;
                        } else {
                            int32_t sw = 0, sh = 0;
                            compositor_surface_get_logical_size(target, &sw, &sh);
                            target->wm_saved_x = target->wm_x;
                            target->wm_saved_y = target->wm_y;
                            target->wm_saved_w = sw;
                            target->wm_saved_h = sh;
                            target->wm_x = 0;
                            target->wm_y = 0;
                            target->wm_maximized = true;
                        }
                        send_toplevel_configure(target);
                    } else {
                        /* Single click: start drag (if maximized, auto-exit maximize first). */
                        if (target->wm_maximized) {
                            target->wm_x = target->wm_saved_x;
                            target->wm_y = target->wm_saved_y;
                            target->wm_maximized = false;
                            if (target->wm_saved_w > 0) target->wm_req_w = target->wm_saved_w;
                            if (target->wm_saved_h > 0) target->wm_req_h = target->wm_saved_h;
                            send_toplevel_configure(target);
                        }
                        srv->wm_drag_surf = target;
                        srv->wm_drag_ptr_start_x = (float)wl_fixed_to_double(fx);
                        srv->wm_drag_ptr_start_y = (float)wl_fixed_to_double(fy);
                        srv->wm_drag_surf_start_x = target->wm_x;
                        srv->wm_drag_surf_start_y = target->wm_y;
                    }
                }
            }

            pointer_send_motion(srv, target, time_ms, lx, ly);
            pointer_send_button(srv, target, time_ms, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
        }
        break;
    }
    case POINTER_ACTION_MOVE: {
        wl_fixed_t old_x = srv->pointer_x, old_y = srv->pointer_y;
        srv->pointer_x = fx;
        srv->pointer_y = fy;
        /* If an interactive resize is active, update requested size (direct mode). */
        if (srv->wm_resize_surf) {
            float px = (float)wl_fixed_to_double(fx);
            float py = (float)wl_fixed_to_double(fy);
            int32_t dx = (int32_t)(px - srv->wm_resize_ptr_start_x);
            int32_t dy = (int32_t)(py - srv->wm_resize_ptr_start_y);
            int32_t nx = srv->wm_resize_start_x;
            int32_t ny = srv->wm_resize_start_y;
            int32_t nw = srv->wm_resize_start_w;
            int32_t nh = srv->wm_resize_start_h;

            if (srv->wm_resize_edges & WM_EDGE_LEFT) { nx += dx; nw -= dx; }
            if (srv->wm_resize_edges & WM_EDGE_RIGHT) { nw += dx; }
            if (srv->wm_resize_edges & WM_EDGE_TOP) { ny += dy; nh -= dy; }
            if (srv->wm_resize_edges & WM_EDGE_BOTTOM) { nh += dy; }

            wm_apply_resize_clamped(srv, srv->wm_resize_surf, nx, ny, nw, nh);
            send_toplevel_configure(srv->wm_resize_surf);
            break;
        }
        /* If an interactive move is active, update the window position instead of
         * forwarding motion to the client (the client already thinks it's dragging). */
        if (srv->wm_drag_surf) {
            float px = (float)wl_fixed_to_double(fx);
            float py = (float)wl_fixed_to_double(fy);
            int32_t nx = srv->wm_drag_surf_start_x + (int32_t)(px - srv->wm_drag_ptr_start_x);
            int32_t ny = srv->wm_drag_surf_start_y + (int32_t)(py - srv->wm_drag_ptr_start_y);
            wm_apply_move_clamped(srv, srv->wm_drag_surf, nx, ny);
            break;
        }
        if (srv->pointer_focus && srv->pointer_focus->resource) {
            wl_fixed_t lx = fx, ly = fy;
            surface_local_coords(srv, srv->pointer_focus, fx, fy, &lx, &ly);
            pointer_send_motion(srv, srv->pointer_focus, time_ms, lx, ly);
            send_relative_motion(srv, srv->pointer_focus, time_ms, fx - old_x, fy - old_y);
        }
        break;
    }
    case POINTER_ACTION_UP:
        srv->pointer_x = fx;
        srv->pointer_y = fy;
        /* End any active interactive move. */
        srv->wm_drag_surf = NULL;
        if (srv->wm_resize_surf) {
            srv->wm_resize_surf->wm_resizing = false;
            send_toplevel_configure(srv->wm_resize_surf);
        }
        srv->wm_resize_surf = NULL;
        srv->wm_resize_edges = 0;
        if (srv->pointer_focus && srv->pointer_focus->resource)
            pointer_send_button(srv, srv->pointer_focus, time_ms, BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
        break;
    default:
        break;
    }

    pthread_mutex_unlock(&srv->surfaces_mutex);
}
