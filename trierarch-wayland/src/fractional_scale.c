/*
 * wp_fractional_scale_manager_v1 / wp_fractional_scale_v1
 *
 * KDE/KWin often relies on fractional-scale + viewporter instead of integer wl_surface.buffer_scale.
 * We publish a preferred scale derived from the compositor output scale.
 */
#include "server_internal.h"
#include "fractional-scale-v1-server-protocol.h"
#include <stdlib.h>

static uint32_t fractional_preferred_scale_120(struct wayland_server *srv) {
    /* Protocol uses numerator with denominator 120. */
    int32_t s = (srv && srv->output_user_scale > 0) ? srv->output_user_scale : 1;
    if (s < 1) s = 1;
    if (s > 4) s = 4;
    return (uint32_t)(s * 120);
}

static void fractional_scale_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    if (surf && surf->fractional_scale_res == resource)
        surf->fractional_scale_res = NULL;
    wl_resource_set_user_data(resource, NULL);
    wl_resource_destroy(resource);
}

static const struct wp_fractional_scale_v1_interface fractional_scale_impl = {
    .destroy = fractional_scale_destroy,
};

static void fractional_scale_resource_destroy(struct wl_resource *resource) {
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    if (surf && surf->fractional_scale_res == resource)
        surf->fractional_scale_res = NULL;
}

static void fractional_scale_manager_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void fractional_scale_manager_get_fractional_scale(struct wl_client *client,
        struct wl_resource *mgr_res, uint32_t id, struct wl_resource *surface_res) {
    struct compositor_surface *surf = wl_resource_get_user_data(surface_res);
    if (!surf) {
        wl_resource_post_error(mgr_res, WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS,
                "invalid surface");
        return;
    }
    if (surf->fractional_scale_res) {
        wl_resource_post_error(mgr_res, WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS,
                "fractional_scale exists");
        return;
    }
    struct wl_resource *fs = wl_resource_create(client, &wp_fractional_scale_v1_interface, 1, id);
    if (!fs) { wl_client_post_no_memory(client); return; }
    surf->fractional_scale_res = fs;
    wl_resource_set_implementation(fs, &fractional_scale_impl, surf, fractional_scale_resource_destroy);

    /* Send initial preferred scale immediately. */
    struct wayland_server *srv = surf->srv;
    wp_fractional_scale_v1_send_preferred_scale(fs, fractional_preferred_scale_120(srv));
}

static const struct wp_fractional_scale_manager_v1_interface fractional_scale_manager_impl = {
    .destroy = fractional_scale_manager_destroy,
    .get_fractional_scale = fractional_scale_manager_get_fractional_scale,
};

void fractional_scale_manager_bind(struct wl_client *client, void *data,
        uint32_t version, uint32_t id) {
    (void)version;
    struct wl_resource *res = wl_resource_create(client, &wp_fractional_scale_manager_v1_interface, 1, id);
    if (!res) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(res, &fractional_scale_manager_impl, data, NULL);
}

void fractional_scale_notify_all(struct wayland_server *srv) {
    if (!srv) return;
    uint32_t scale120 = fractional_preferred_scale_120(srv);
    pthread_mutex_lock(&srv->surfaces_mutex);
    struct compositor_surface *surf;
    wl_list_for_each(surf, &srv->surfaces, link) {
        if (!surf->fractional_scale_res) continue;
        wp_fractional_scale_v1_send_preferred_scale(surf->fractional_scale_res, scale120);
    }
    pthread_mutex_unlock(&srv->surfaces_mutex);
}

