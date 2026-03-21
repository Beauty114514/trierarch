/*
 * zxdg_decoration_manager_v1: server-side decoration negotiation.
 * Always responds SERVER_SIDE so clients skip drawing title bars.
 */
#include "server_internal.h"
#include "xdg-decoration-unstable-v1-server-protocol.h"
#include <stdlib.h>

static void toplevel_decoration_destroy(struct wl_client *c, struct wl_resource *r) {
    (void)c; wl_resource_destroy(r);
}

static void toplevel_decoration_set_mode(struct wl_client *c, struct wl_resource *r, uint32_t mode) {
    (void)c; (void)mode;
    zxdg_toplevel_decoration_v1_send_configure(r, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static void toplevel_decoration_unset_mode(struct wl_client *c, struct wl_resource *r) {
    (void)c;
    zxdg_toplevel_decoration_v1_send_configure(r, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static const struct zxdg_toplevel_decoration_v1_interface toplevel_decoration_impl = {
    .destroy = toplevel_decoration_destroy,
    .set_mode = toplevel_decoration_set_mode,
    .unset_mode = toplevel_decoration_unset_mode,
};

static void decoration_manager_destroy(struct wl_client *c, struct wl_resource *r) {
    (void)c; wl_resource_destroy(r);
}

static void decoration_manager_get_toplevel_decoration(struct wl_client *client,
        struct wl_resource *resource, uint32_t id, struct wl_resource *toplevel) {
    (void)resource; (void)toplevel;
    struct wl_resource *deco = wl_resource_create(client,
            &zxdg_toplevel_decoration_v1_interface, 1, id);
    if (!deco) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(deco, &toplevel_decoration_impl, NULL, NULL);
    zxdg_toplevel_decoration_v1_send_configure(deco, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

static const struct zxdg_decoration_manager_v1_interface decoration_manager_impl = {
    .destroy = decoration_manager_destroy,
    .get_toplevel_decoration = decoration_manager_get_toplevel_decoration,
};

void xdg_decoration_manager_bind(struct wl_client *client, void *data,
        uint32_t version, uint32_t id) {
    (void)data;
    struct wl_resource *res = wl_resource_create(client,
            &zxdg_decoration_manager_v1_interface, version < 1 ? version : 1, id);
    if (!res) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(res, &decoration_manager_impl, data, NULL);
}
