/*
 * wl_output: global bind, geometry/mode/scale/done. Output size/scale/override logic.
 */
#include "server_internal.h"
#include "compositor.h"
#include <stdlib.h>

/*
 * Output sizing contract:
 * - Kotlin provides physical surface size and two tuning knobs:
 *   - "Resolution%" lowers the logical output size to reduce compositor work.
 *   - "Scale%" enlarges UI by reducing the logical coordinate space, and also hints clients to
 *     render denser buffers for crispness (fractional-scale + viewporter).
 * - The compositor publishes the resulting logical size via wl_output and xdg-shell configure,
 *   and provides an output_scale for the renderer to map logical -> physical.
 */

static void output_resource_destroy(struct wl_listener *listener, void *data) {
    (void)data;
    struct output_resource_node *node = wl_container_of(listener, node, destroy_listener);
    wl_list_remove(&node->link);
    wl_list_remove(&node->destroy_listener.link);
    free(node);
}

static void output_handle_release(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct wl_output_interface output_impl = {
    .release = output_handle_release,
};

/* Send current geometry/mode/scale to one wl_output resource; if send_done, send done (for notify). */
static void output_send_state(struct wayland_server *srv, struct wl_resource *resource, bool send_done) {
    int32_t w = srv->output_width > 0 ? srv->output_width : 1080;
    int32_t h = srv->output_height > 0 ? srv->output_height : 1920;
    uint32_t ver = (uint32_t)wl_resource_get_version(resource);
    int32_t mm_w = w * 254 / 1000;
    int32_t mm_h = h * 254 / 1000;
    wl_output_send_geometry(resource, 0, 0, mm_w, mm_h,
            WL_OUTPUT_SUBPIXEL_UNKNOWN, "Trierarch", "Wayland", WL_OUTPUT_TRANSFORM_NORMAL);
    wl_output_send_mode(resource, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED, w, h, 60000);
    if (ver >= 2) {
        int32_t scale = srv->output_user_scale > 0 ? srv->output_user_scale : 1;
        wl_output_send_scale(resource, (uint32_t)scale);
    }
    if (send_done && ver >= 2)
        wl_output_send_done(resource);
}

/* Notify all already-bound wl_output resources (geometry+mode+scale+done) so Resolution takes effect. */
static void output_notify_all(struct wayland_server *srv) {
    struct output_resource_node *node;
    wl_list_for_each(node, &srv->output_resources, link) {
        if (node->resource)
            output_send_state(srv, node->resource, true);
    }
}

void output_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct wayland_server *srv = data;
    uint32_t bind_ver = version < 4 ? version : 4;
    struct wl_resource *resource = wl_resource_create(client, &wl_output_interface, bind_ver, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &output_impl, srv, NULL);

    struct output_resource_node *node = calloc(1, sizeof(*node));
    if (node) {
        node->resource = resource;
        node->destroy_listener.notify = output_resource_destroy;
        wl_resource_add_destroy_listener(resource, &node->destroy_listener);
        wl_list_insert(&srv->output_resources, &node->link);
    }

    output_send_state(srv, resource, false);
    if (bind_ver >= 4) {
        wl_output_send_name(resource, "Trierarch-1");
        wl_output_send_description(resource, "Trierarch Wayland Output");
    }
    if (bind_ver >= 2)
        wl_output_send_done(resource);
}

void compositor_set_output_size(wayland_server_t *srv_opaque,
        int32_t logical_w, int32_t logical_h, int32_t physical_w, int32_t physical_h) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    if (srv->output_override_w > 0 && srv->output_override_h > 0) {
        srv->output_width = srv->output_override_w;
        srv->output_height = srv->output_override_h;
    } else {
        srv->output_width = logical_w > 0 ? logical_w : physical_w;
        srv->output_height = logical_h > 0 ? logical_h : physical_h;
    }
    /* output_scale = fill scale (physical/logical) for renderer and wl_output */
    if (physical_w > 0 && physical_h > 0 && srv->output_width > 0 && srv->output_height > 0) {
        int32_t scale_w = physical_w / srv->output_width;
        int32_t scale_h = physical_h / srv->output_height;
        srv->output_scale = (scale_w < scale_h) ? scale_w : scale_h;
        if (srv->output_scale < 1) srv->output_scale = 1;
    }
    pthread_mutex_lock(&srv->surfaces_mutex);
    struct compositor_surface *surf;
    wl_list_for_each(surf, &srv->surfaces, link) {
        if (surf->xdg_toplevel_res)
            send_toplevel_configure(surf);
    }
    pthread_mutex_unlock(&srv->surfaces_mutex);
    /* Notify already-bound wl_output resources so Resolution takes effect without reconnecting. */
    output_notify_all(srv);
    xdg_output_notify_all(srv);
    surface_notify_preferred_buffer_scale_all(srv);
    fractional_scale_notify_all(srv);
}

void compositor_set_output_override(wayland_server_t *srv_opaque, int32_t w, int32_t h) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    srv->output_override_w = w;
    srv->output_override_h = h;
}

void compositor_get_output_size(wayland_server_t *srv_opaque, int32_t *w, int32_t *h) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    if (w) *w = srv->output_width;
    if (h) *h = srv->output_height;
}

int32_t compositor_get_output_scale(wayland_server_t *srv_opaque) {
    if (!srv_opaque) return 1;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    return srv->output_scale > 0 ? srv->output_scale : 1;
}

void compositor_set_output_user_scale(wayland_server_t *srv_opaque, int32_t scale) {
    if (!srv_opaque) return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    if (scale < 1) scale = 1;
    if (scale > 10) scale = 10;
    srv->output_user_scale = scale;
    pthread_mutex_lock(&srv->surfaces_mutex);
    struct compositor_surface *surf;
    wl_list_for_each(surf, &srv->surfaces, link) {
        if (surf->xdg_toplevel_res)
            send_toplevel_configure(surf);
    }
    pthread_mutex_unlock(&srv->surfaces_mutex);
    output_notify_all(srv);
    xdg_output_notify_all(srv);
    surface_notify_preferred_buffer_scale_all(srv);
    fractional_scale_notify_all(srv);
}

