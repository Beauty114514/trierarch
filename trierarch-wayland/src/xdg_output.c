/*
 * zxdg_output_manager_v1 / zxdg_output_v1: logical output info.
 * Nested compositors (KWin) need this for output layout.
 */
#include "server_internal.h"
#include "xdg-output-unstable-v1-server-protocol.h"
#include <stdlib.h>

static void xdg_output_destroy(struct wl_client *c, struct wl_resource *r) {
    (void)c; wl_resource_destroy(r);
}

static const struct zxdg_output_v1_interface xdg_output_impl = {
    .destroy = xdg_output_destroy,
};

static void xdg_output_manager_destroy(struct wl_client *c, struct wl_resource *r) {
    (void)c; wl_resource_destroy(r);
}

static void xdg_output_manager_get_xdg_output(struct wl_client *client,
        struct wl_resource *resource, uint32_t id, struct wl_resource *output) {
    (void)output;
    struct wayland_server *srv = wl_resource_get_user_data(resource);
    uint32_t ver = wl_resource_get_version(resource);
    struct wl_resource *xo = wl_resource_create(client, &zxdg_output_v1_interface, ver, id);
    if (!xo) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(xo, &xdg_output_impl, srv, NULL);
    int32_t w = srv && srv->output_width > 0 ? srv->output_width : 1080;
    int32_t h = srv && srv->output_height > 0 ? srv->output_height : 1920;
    zxdg_output_v1_send_logical_position(xo, 0, 0);
    zxdg_output_v1_send_logical_size(xo, w, h);
    if (ver >= 2) {
        zxdg_output_v1_send_name(xo, "Trierarch-1");
        zxdg_output_v1_send_description(xo, "Trierarch Wayland Output");
    }
    if (ver < 3)
        zxdg_output_v1_send_done(xo);
}

static const struct zxdg_output_manager_v1_interface xdg_output_manager_impl = {
    .destroy = xdg_output_manager_destroy,
    .get_xdg_output = xdg_output_manager_get_xdg_output,
};

void xdg_output_manager_bind(struct wl_client *client, void *data,
        uint32_t version, uint32_t id) {
    uint32_t ver = version < 3 ? version : 3;
    struct wl_resource *res = wl_resource_create(client,
            &zxdg_output_manager_v1_interface, ver, id);
    if (!res) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(res, &xdg_output_manager_impl, data, NULL);
}
