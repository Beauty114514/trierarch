/*
 * xdg_wm_base, xdg_surface, xdg_toplevel, xdg_popup, xdg_positioner.
 *
 * WM_MODE_NESTED : toplevels are configured fullscreen; popup positions not tracked.
 * WM_MODE_DIRECT : toplevels get windowed configure; popup positions are computed from
 *                  the positioner and placed in absolute output coords.
 */
#include "server_internal.h"
#include "xdg-shell-server-protocol.h"
#include <android/log.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "TrierarchXdgShell"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* --- xdg_positioner state ------------------------------------------------- */

struct positioner_state {
    int32_t size_w, size_h;        /* requested popup size */
    int32_t anchor_x, anchor_y;    /* anchor rect origin relative to parent surface */
    int32_t anchor_w, anchor_h;    /* anchor rect size */
    uint32_t anchor;               /* XDG_POSITIONER_ANCHOR_* */
    uint32_t gravity;              /* XDG_POSITIONER_GRAVITY_* */
    int32_t offset_x, offset_y;    /* additional x/y offset */
};

/* Compute the popup's top-left position in parent-surface-local coords.
 * Follows xdg-shell spec §anchor/gravity logic (simplified: no constraint adjustment). */
static void positioner_compute_local(const struct positioner_state *p,
        int32_t *out_x, int32_t *out_y) {
    /* Anchor point on the anchor rect. */
    int32_t ax = p->anchor_x;
    int32_t ay = p->anchor_y;
    uint32_t anchor = p->anchor;
    if (anchor == XDG_POSITIONER_ANCHOR_TOP || anchor == XDG_POSITIONER_ANCHOR_TOP_LEFT
            || anchor == XDG_POSITIONER_ANCHOR_TOP_RIGHT)
        ay = p->anchor_y;
    else if (anchor == XDG_POSITIONER_ANCHOR_BOTTOM || anchor == XDG_POSITIONER_ANCHOR_BOTTOM_LEFT
            || anchor == XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)
        ay = p->anchor_y + p->anchor_h;
    else
        ay = p->anchor_y + p->anchor_h / 2;

    if (anchor == XDG_POSITIONER_ANCHOR_LEFT || anchor == XDG_POSITIONER_ANCHOR_TOP_LEFT
            || anchor == XDG_POSITIONER_ANCHOR_BOTTOM_LEFT)
        ax = p->anchor_x;
    else if (anchor == XDG_POSITIONER_ANCHOR_RIGHT || anchor == XDG_POSITIONER_ANCHOR_TOP_RIGHT
            || anchor == XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)
        ax = p->anchor_x + p->anchor_w;
    else
        ax = p->anchor_x + p->anchor_w / 2;

    /* Gravity: which direction the popup extends from the anchor point. */
    int32_t px = ax, py = ay;
    uint32_t gravity = p->gravity;
    if (gravity == XDG_POSITIONER_GRAVITY_BOTTOM || gravity == XDG_POSITIONER_GRAVITY_BOTTOM_LEFT
            || gravity == XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
        py = ay;                         /* popup top aligns to anchor */
    else if (gravity == XDG_POSITIONER_GRAVITY_TOP || gravity == XDG_POSITIONER_GRAVITY_TOP_LEFT
            || gravity == XDG_POSITIONER_GRAVITY_TOP_RIGHT)
        py = ay - p->size_h;             /* popup bottom aligns to anchor */
    else
        py = ay - p->size_h / 2;        /* centred vertically */

    if (gravity == XDG_POSITIONER_GRAVITY_RIGHT || gravity == XDG_POSITIONER_GRAVITY_TOP_RIGHT
            || gravity == XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
        px = ax;                         /* popup left aligns to anchor */
    else if (gravity == XDG_POSITIONER_GRAVITY_LEFT || gravity == XDG_POSITIONER_GRAVITY_TOP_LEFT
            || gravity == XDG_POSITIONER_GRAVITY_BOTTOM_LEFT)
        px = ax - p->size_w;             /* popup right aligns to anchor */
    else
        px = ax - p->size_w / 2;        /* centred horizontally */

    *out_x = px + p->offset_x;
    *out_y = py + p->offset_y;
}

static void wm_base_resource_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct wm_base_resource_node *node = wl_container_of(listener, node, destroy_listener);
    wl_list_remove(&node->link);
    wl_list_remove(&node->destroy_listener.link);
    free(node);
}

static void xdg_surface_resource_destroy(struct wl_resource *resource) {
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    if (surf) surf->xdg_surface_res = NULL;
}
static void xdg_toplevel_resource_destroy(struct wl_resource *resource) {
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    if (surf) surf->xdg_toplevel_res = NULL;
}

static void xdg_toplevel_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}
static void xdg_toplevel_set_parent(struct wl_client *c, struct wl_resource *r, struct wl_resource *p) {
    (void)c;(void)r;(void)p;
}
static void xdg_toplevel_set_title(struct wl_client *c, struct wl_resource *r, const char *t) {
    (void)c;
    if (!t) return;
    struct compositor_surface *surf = wl_resource_get_user_data(r);
    if (!surf) return;
    int len = 0;
    while (t[len] && len < 254) len++;
    for (int i = 0; i < len; i++) surf->title[i] = t[i];
    surf->title[len] = '\0';
}
static void xdg_toplevel_set_app_id(struct wl_client *c, struct wl_resource *r, const char *a) {
    (void)c;
    if (!a) return;
    struct compositor_surface *surf = wl_resource_get_user_data(r);
    if (!surf) return;
    int len = 0;
    while (a[len] && len < 254) len++;
    for (int i = 0; i < len; i++) surf->app_id[i] = a[i];
    surf->app_id[len] = '\0';
}
static void xdg_toplevel_show_window_menu(struct wl_client *c, struct wl_resource *r,
        struct wl_resource *s, uint32_t serial, int32_t x, int32_t y) {
    (void)c;(void)r;(void)s;(void)serial;(void)x;(void)y;
}
static void xdg_toplevel_move(struct wl_client *c, struct wl_resource *r,
        struct wl_resource *s, uint32_t serial) {
    (void)c;(void)s;(void)serial;
    struct compositor_surface *surf = wl_resource_get_user_data(r);
    if (!surf || !surf->srv || surf->srv->wm_mode != WM_MODE_DIRECT) return;
    struct wayland_server *srv = surf->srv;
    /* Record the drag start: current pointer position and surface position. */
    srv->wm_drag_surf = surf;
    srv->wm_drag_ptr_start_x = (float)wl_fixed_to_double(srv->pointer_x);
    srv->wm_drag_ptr_start_y = (float)wl_fixed_to_double(srv->pointer_y);
    srv->wm_drag_surf_start_x = surf->wm_x;
    srv->wm_drag_surf_start_y = surf->wm_y;
}
static void xdg_toplevel_resize(struct wl_client *c, struct wl_resource *r,
        struct wl_resource *s, uint32_t edge, uint32_t serial) {
    (void)c;(void)r;(void)s;(void)edge;(void)serial;
}
static void xdg_toplevel_set_max_size(struct wl_client *c, struct wl_resource *r, int32_t w, int32_t h) {
    (void)c;(void)r;(void)w;(void)h;
}
static void xdg_toplevel_set_min_size(struct wl_client *c, struct wl_resource *r, int32_t w, int32_t h) {
    (void)c;(void)r;(void)w;(void)h;
}
static void xdg_toplevel_set_maximized(struct wl_client *c, struct wl_resource *r) {
    (void)c;
    struct compositor_surface *surf = wl_resource_get_user_data(r);
    if (!surf) return;
    if (!surf->wm_maximized) {
        int32_t sw = 0, sh = 0;
        compositor_surface_get_logical_size(surf, &sw, &sh);
        surf->wm_saved_x = surf->wm_x;
        surf->wm_saved_y = surf->wm_y;
        surf->wm_saved_w = sw;
        surf->wm_saved_h = sh;
        surf->wm_x = 0;
        surf->wm_y = 0;
        surf->wm_maximized = true;
    }
    send_toplevel_configure(surf);
}
static void xdg_toplevel_unset_maximized(struct wl_client *c, struct wl_resource *r) {
    (void)c;
    struct compositor_surface *surf = wl_resource_get_user_data(r);
    if (!surf) return;
    if (surf->wm_maximized) {
        surf->wm_x = surf->wm_saved_x;
        surf->wm_y = surf->wm_saved_y;
        surf->wm_maximized = false;
    }
    send_toplevel_configure(surf);
}
static void xdg_toplevel_set_fullscreen(struct wl_client *c, struct wl_resource *r, struct wl_resource *o) {
    (void)c;(void)o;
    struct compositor_surface *surf = wl_resource_get_user_data(r);
    if (!surf) return;
    if (!surf->wm_maximized) {
        int32_t sw = 0, sh = 0;
        compositor_surface_get_logical_size(surf, &sw, &sh);
        surf->wm_saved_x = surf->wm_x;
        surf->wm_saved_y = surf->wm_y;
        surf->wm_saved_w = sw;
        surf->wm_saved_h = sh;
        surf->wm_x = 0;
        surf->wm_y = 0;
        surf->wm_maximized = true;
    }
    send_toplevel_configure(surf);
}
static void xdg_toplevel_unset_fullscreen(struct wl_client *c, struct wl_resource *r) {
    (void)c;
    struct compositor_surface *surf = wl_resource_get_user_data(r);
    if (!surf) return;
    if (surf->wm_maximized) {
        surf->wm_x = surf->wm_saved_x;
        surf->wm_y = surf->wm_saved_y;
        surf->wm_maximized = false;
    }
    send_toplevel_configure(surf);
}
static void xdg_toplevel_set_minimized(struct wl_client *c, struct wl_resource *r) {
    (void)c;(void)r;
}

static const struct xdg_toplevel_interface xdg_toplevel_impl = {
    .destroy = xdg_toplevel_destroy,
    .set_parent = xdg_toplevel_set_parent,
    .set_title = xdg_toplevel_set_title,
    .set_app_id = xdg_toplevel_set_app_id,
    .show_window_menu = xdg_toplevel_show_window_menu,
    .move = xdg_toplevel_move,
    .resize = xdg_toplevel_resize,
    .set_max_size = xdg_toplevel_set_max_size,
    .set_min_size = xdg_toplevel_set_min_size,
    .set_maximized = xdg_toplevel_set_maximized,
    .unset_maximized = xdg_toplevel_unset_maximized,
    .set_fullscreen = xdg_toplevel_set_fullscreen,
    .unset_fullscreen = xdg_toplevel_unset_fullscreen,
    .set_minimized = xdg_toplevel_set_minimized,
};

void send_toplevel_configure(struct compositor_surface *surf) {
    if (!surf || !surf->xdg_toplevel_res || !surf->xdg_surface_res || !surf->srv) return;
    struct wl_array states;
    wl_array_init(&states);
    uint32_t *s_act = wl_array_add(&states, sizeof(uint32_t));
    if (s_act) *s_act = XDG_TOPLEVEL_STATE_ACTIVATED;
    int32_t w = 0, h = 0;

    if (surf->srv->wm_mode == WM_MODE_NESTED) {
        /* Nested mode: tell the client (KDE/Sway) to occupy the full output.
         * FULLSCREEN + MAXIMIZED prevents it from drawing decorations and ensures
         * it allocates a buffer that matches the output size. */
        w = surf->srv->output_width  > 0 ? surf->srv->output_width  : 0;
        h = surf->srv->output_height > 0 ? surf->srv->output_height : 0;
        uint32_t *s_fs  = wl_array_add(&states, sizeof(uint32_t));
        if (s_fs)  *s_fs  = XDG_TOPLEVEL_STATE_FULLSCREEN;
        uint32_t *s_max = wl_array_add(&states, sizeof(uint32_t));
        if (s_max) *s_max = XDG_TOPLEVEL_STATE_MAXIMIZED;
    } else {
        /* Direct mode: let the client choose its own size (w=0, h=0 per xdg-shell spec).
         * If explicitly maximized, fill the output and advertise MAXIMIZED. */
        if (surf->wm_resizing) {
            uint32_t *s_rz = wl_array_add(&states, sizeof(uint32_t));
            if (s_rz) *s_rz = XDG_TOPLEVEL_STATE_RESIZING;
        }
        if (surf->wm_maximized) {
            w = surf->srv->output_width  > 0 ? surf->srv->output_width  : 0;
            h = surf->srv->output_height > 0 ? surf->srv->output_height : 0;
            uint32_t *s_max = wl_array_add(&states, sizeof(uint32_t));
            if (s_max) *s_max = XDG_TOPLEVEL_STATE_MAXIMIZED;
        } else if (surf->wm_req_w > 0 || surf->wm_req_h > 0) {
            /* Compositor-driven resize: send requested size (best-effort). */
            w = surf->wm_req_w > 0 ? surf->wm_req_w : 0;
            h = surf->wm_req_h > 0 ? surf->wm_req_h : 0;
        }
    }

    xdg_toplevel_send_configure(surf->xdg_toplevel_res, w, h, &states);
    wl_array_release(&states);
    uint32_t serial = wl_display_next_serial(surf->srv->display);
    xdg_surface_send_configure(surf->xdg_surface_res, serial);
}

static void xdg_surface_get_toplevel(struct wl_client *client, struct wl_resource *xdg_surface_res, uint32_t id) {
    struct compositor_surface *surf = wl_resource_get_user_data(xdg_surface_res);
    if (!surf) return;
    if (surf->xdg_toplevel_res) {
        wl_resource_post_error(xdg_surface_res, XDG_WM_BASE_ERROR_ROLE, "surface already has role");
        return;
    }
    struct wl_resource *toplevel = wl_resource_create(client, &xdg_toplevel_interface,
            wl_resource_get_version(xdg_surface_res), id);
    if (!toplevel) {
        wl_client_post_no_memory(client);
        return;
    }
    surf->xdg_toplevel_res = toplevel;
    wl_resource_set_implementation(toplevel, &xdg_toplevel_impl, surf, xdg_toplevel_resource_destroy);

    /* Assign a cascaded initial position and a unique stacking z_order. */
    if (surf->srv) {
        surf->wm_x = surf->srv->cascade_x;
        surf->wm_y = surf->srv->cascade_y;
        surf->z_order = surf->srv->next_z_order++;
        surf->srv->cascade_x += 40;
        surf->srv->cascade_y += 40;
        /* Wrap cascade within the top-left quarter so windows stay accessible. */
        int32_t max_x = surf->srv->output_width / 4;
        int32_t max_y = surf->srv->output_height / 4;
        if (max_x < 40) max_x = 40;
        if (max_y < 40) max_y = 40;
        if (surf->srv->cascade_x >= max_x) surf->srv->cascade_x = 0;
        if (surf->srv->cascade_y >= max_y) surf->srv->cascade_y = 0;
    }

    send_toplevel_configure(surf);
}

static void xdg_popup_destroy(struct wl_client *c, struct wl_resource *r) { (void)c; wl_resource_destroy(r); }
static void xdg_popup_grab(struct wl_client *c, struct wl_resource *r, struct wl_resource *s, uint32_t serial) {
    (void)c;(void)r;(void)s;(void)serial;
}
static void xdg_popup_reposition(struct wl_client *c, struct wl_resource *r, struct wl_resource *p, uint32_t token) {
    (void)c;(void)r;(void)p;(void)token;
}
static const struct xdg_popup_interface xdg_popup_impl = {
    .destroy = xdg_popup_destroy,
    .grab = xdg_popup_grab,
    .reposition = xdg_popup_reposition,
};

static void xdg_surface_get_popup(struct wl_client *client, struct wl_resource *xdg_surface_res,
        uint32_t id, struct wl_resource *parent_res, struct wl_resource *positioner_res) {
    struct compositor_surface *surf = wl_resource_get_user_data(xdg_surface_res);
    struct wl_resource *popup = wl_resource_create(client, &xdg_popup_interface,
            wl_resource_get_version(xdg_surface_res), id);
    if (!popup) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(popup, &xdg_popup_impl, surf, NULL);

    /* Compute popup position from positioner + parent surface screen position. */
    int32_t popup_x = 0, popup_y = 0;
    int32_t popup_w = 1, popup_h = 1;

    struct positioner_state *pos = positioner_res
            ? wl_resource_get_user_data(positioner_res) : NULL;
    if (pos) {
        popup_w = pos->size_w > 0 ? pos->size_w : 1;
        popup_h = pos->size_h > 0 ? pos->size_h : 1;
        positioner_compute_local(pos, &popup_x, &popup_y);
    }

    /* Translate from parent-surface-local to screen coords.
     * parent_res is the xdg_surface of the parent; its compositor_surface has wm_x/wm_y. */
    if (parent_res && surf && surf->srv && surf->srv->wm_mode == WM_MODE_DIRECT) {
        struct compositor_surface *parent_surf = wl_resource_get_user_data(parent_res);
        if (parent_surf) {
            surf->wm_x = parent_surf->wm_x + popup_x;
            surf->wm_y = parent_surf->wm_y + popup_y;
            /* Popup stacks above its parent. */
            surf->z_order = parent_surf->z_order + 500;
        }
    }

    xdg_popup_send_configure(popup, popup_x, popup_y, popup_w, popup_h);
    uint32_t serial = wl_display_next_serial(wl_client_get_display(client));
    xdg_surface_send_configure(xdg_surface_res, serial);
}

static void xdg_surface_set_window_geometry(struct wl_client *c, struct wl_resource *r,
        int32_t x, int32_t y, int32_t w, int32_t h) {
    (void)c;
    static unsigned geom_logged;
    struct compositor_surface *surf = wl_resource_get_user_data(r);
    if (geom_logged < 64) {
        LOGI("xdg_surface.set_window_geometry surf=%p app_id=%s title=%s geom=%d,%d %dx%d",
                (void *)surf,
                surf ? surf->app_id : "?",
                surf ? surf->title : "?",
                (int)x, (int)y, (int)w, (int)h);
        geom_logged++;
    }
}
static void xdg_surface_ack_configure(struct wl_client *c, struct wl_resource *r, uint32_t serial) {
    (void)c;
    static unsigned ack_logged;
    struct compositor_surface *surf = wl_resource_get_user_data(r);
    if (ack_logged < 128) {
        LOGI("xdg_surface.ack_configure surf=%p app_id=%s title=%s serial=%u",
                (void *)surf,
                surf ? surf->app_id : "?",
                surf ? surf->title : "?",
                (unsigned)serial);
        ack_logged++;
    }
}
static void xdg_surface_destroy_req(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct xdg_surface_interface xdg_surface_impl = {
    .destroy = xdg_surface_destroy_req,
    .get_toplevel = xdg_surface_get_toplevel,
    .get_popup = xdg_surface_get_popup,
    .set_window_geometry = xdg_surface_set_window_geometry,
    .ack_configure = xdg_surface_ack_configure,
};

static void xdg_surface_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void xdg_wm_base_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}
static void pos_destroy(struct wl_client *c, struct wl_resource *r) {
    (void)c;
    wl_resource_destroy(r);
}
static void pos_resource_destroy(struct wl_resource *r) {
    struct positioner_state *p = wl_resource_get_user_data(r);
    free(p);
}
static void pos_set_size(struct wl_client *c, struct wl_resource *r, int32_t w, int32_t h) {
    (void)c;
    struct positioner_state *p = wl_resource_get_user_data(r);
    if (p) { p->size_w = w; p->size_h = h; }
}
static void pos_set_anchor_rect(struct wl_client *c, struct wl_resource *r,
        int32_t x, int32_t y, int32_t w, int32_t h) {
    (void)c;
    struct positioner_state *p = wl_resource_get_user_data(r);
    if (p) { p->anchor_x = x; p->anchor_y = y; p->anchor_w = w; p->anchor_h = h; }
}
static void pos_set_anchor(struct wl_client *c, struct wl_resource *r, uint32_t a) {
    (void)c;
    struct positioner_state *p = wl_resource_get_user_data(r);
    if (p) p->anchor = a;
}
static void pos_set_gravity(struct wl_client *c, struct wl_resource *r, uint32_t g) {
    (void)c;
    struct positioner_state *p = wl_resource_get_user_data(r);
    if (p) p->gravity = g;
}
static void pos_set_constraint_adjustment(struct wl_client *c, struct wl_resource *r, uint32_t a) {
    (void)c;(void)r;(void)a; /* constraint flip/slide not implemented; menus usually still visible */
}
static void pos_set_offset(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y) {
    (void)c;
    struct positioner_state *p = wl_resource_get_user_data(r);
    if (p) { p->offset_x = x; p->offset_y = y; }
}
static void pos_set_reactive(struct wl_client *c, struct wl_resource *r) { (void)c;(void)r; }
static void pos_set_parent_size(struct wl_client *c, struct wl_resource *r, int32_t w, int32_t h) {
    (void)c;(void)r;(void)w;(void)h;
}
static void pos_set_parent_configure(struct wl_client *c, struct wl_resource *r, uint32_t s) {
    (void)c;(void)r;(void)s;
}

static const struct xdg_positioner_interface xdg_positioner_impl = {
    .destroy = pos_destroy,
    .set_size = pos_set_size,
    .set_anchor_rect = pos_set_anchor_rect,
    .set_anchor = pos_set_anchor,
    .set_gravity = pos_set_gravity,
    .set_constraint_adjustment = pos_set_constraint_adjustment,
    .set_offset = pos_set_offset,
    .set_reactive = pos_set_reactive,
    .set_parent_size = pos_set_parent_size,
    .set_parent_configure = pos_set_parent_configure,
};

static void xdg_wm_base_create_positioner(struct wl_client *client, struct wl_resource *resource, uint32_t id) {
    struct positioner_state *p = calloc(1, sizeof(*p));
    if (!p) { wl_client_post_no_memory(client); return; }
    struct wl_resource *pos = wl_resource_create(client, &xdg_positioner_interface,
            wl_resource_get_version(resource), id);
    if (!pos) {
        free(p);
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(pos, &xdg_positioner_impl, p, pos_resource_destroy);
}
static void xdg_wm_base_get_xdg_surface(struct wl_client *client, struct wl_resource *wm_res, uint32_t id,
        struct wl_resource *surface_res) {
    struct wayland_server *srv = wl_resource_get_user_data(wm_res);
    struct compositor_surface *surf = wl_resource_get_user_data(surface_res);
    if (!surf || surf->srv != srv) {
        wl_resource_post_error(wm_res, XDG_WM_BASE_ERROR_INVALID_SURFACE_STATE, "invalid surface");
        return;
    }
    if (surf->xdg_surface_res) {
        wl_resource_post_error(wm_res, XDG_WM_BASE_ERROR_ROLE, "surface already has xdg_surface");
        return;
    }
    struct wl_resource *xdg_surf = wl_resource_create(client, &xdg_surface_interface,
            wl_resource_get_version(wm_res), id);
    if (!xdg_surf) {
        wl_client_post_no_memory(client);
        return;
    }
    surf->xdg_surface_res = xdg_surf;
    wl_resource_set_implementation(xdg_surf, &xdg_surface_impl, surf, xdg_surface_resource_destroy);
    {
        static unsigned g_get_xdg_surface_logged;
        if (g_get_xdg_surface_logged < 128) {
            pid_t pid = (pid_t)-1;
            uid_t uid = 0;
            gid_t gid = 0;
            wl_client_get_credentials(client, &pid, &uid, &gid);
            LOGI("xdg_wm_base.get_xdg_surface: surf=%p pid=%d uid=%u (n=%u)",
                    (void *)surf, (int)pid, (unsigned)uid, g_get_xdg_surface_logged + 1);
            g_get_xdg_surface_logged++;
        }
    }
}
static void xdg_wm_base_pong(struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
    (void)client;(void)resource;(void)serial;
}

static const struct xdg_wm_base_interface xdg_wm_base_impl = {
    .destroy = xdg_wm_base_destroy,
    .create_positioner = xdg_wm_base_create_positioner,
    .get_xdg_surface = xdg_wm_base_get_xdg_surface,
    .pong = xdg_wm_base_pong,
};

void xdg_shell_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct wayland_server *srv = data;
    struct wl_resource *resource = wl_resource_create(client, &xdg_wm_base_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &xdg_wm_base_impl, srv, NULL);
    {
        static unsigned g_bind_logged;
        if (g_bind_logged < 128) {
            pid_t pid = (pid_t)-1;
            uid_t uid = 0;
            gid_t gid = 0;
            wl_client_get_credentials(client, &pid, &uid, &gid);
            LOGI("bind xdg_wm_base v%u pid=%d uid=%u (n=%u)", version, (int)pid, (unsigned)uid, g_bind_logged + 1);
            g_bind_logged++;
        }
    }

    struct wm_base_resource_node *node = calloc(1, sizeof(*node));
    if (node) {
        node->resource = resource;
        node->destroy_listener.notify = wm_base_resource_destroy;
        wl_resource_add_destroy_listener(resource, &node->destroy_listener);
        wl_list_insert(&srv->wm_base_resources, &node->link);
    }
}
