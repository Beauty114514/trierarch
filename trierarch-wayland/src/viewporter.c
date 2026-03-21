/*
 * wp_viewporter / wp_viewport: surface source-rect and destination-size.
 * Required by KDE/KWin for scaling single-pixel buffers and surface cropping.
 */
#include "server_internal.h"
#include "viewporter-server-protocol.h"
#include <stdlib.h>

static void viewport_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    if (surf && surf->viewport_res == resource)
        surf->viewport_res = NULL;
    wl_resource_set_user_data(resource, NULL);
    wl_resource_destroy(resource);
}

static void viewport_set_source(struct wl_client *client, struct wl_resource *resource,
        wl_fixed_t x, wl_fixed_t y, wl_fixed_t width, wl_fixed_t height) {
    (void)client; (void)resource; (void)x; (void)y; (void)width; (void)height;
}

static void viewport_set_destination(struct wl_client *client, struct wl_resource *resource,
        int32_t width, int32_t height) {
    (void)client; (void)resource; (void)width; (void)height;
}

static const struct wp_viewport_interface viewport_impl = {
    .destroy = viewport_destroy,
    .set_source = viewport_set_source,
    .set_destination = viewport_set_destination,
};

static void viewport_resource_destroy(struct wl_resource *resource) {
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    if (surf && surf->viewport_res == resource)
        surf->viewport_res = NULL;
}

static void viewporter_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void viewporter_get_viewport(struct wl_client *client, struct wl_resource *vp_res,
        uint32_t id, struct wl_resource *surface_res) {
    struct compositor_surface *surf = wl_resource_get_user_data(surface_res);
    if (!surf) {
        wl_resource_post_error(vp_res, WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS, "invalid surface");
        return;
    }
    if (surf->viewport_res) {
        wl_resource_post_error(vp_res, WP_VIEWPORTER_ERROR_VIEWPORT_EXISTS, "viewport exists");
        return;
    }
    struct wl_resource *port = wl_resource_create(client, &wp_viewport_interface, 1, id);
    if (!port) { wl_client_post_no_memory(client); return; }
    surf->viewport_res = port;
    wl_resource_set_implementation(port, &viewport_impl, surf, viewport_resource_destroy);
}

static const struct wp_viewporter_interface viewporter_impl = {
    .destroy = viewporter_destroy,
    .get_viewport = viewporter_get_viewport,
};

void viewporter_bind(struct wl_client *client, void *data,
        uint32_t version, uint32_t id) {
    (void)version;
    struct wl_resource *res = wl_resource_create(client, &wp_viewporter_interface, 1, id);
    if (!res) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(res, &viewporter_impl, data, NULL);
}
