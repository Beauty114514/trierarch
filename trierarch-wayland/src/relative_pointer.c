/*
 * zwp_relative_pointer_manager_v1: delta pointer motion events.
 *
 * Why this exists:
 * - Some compositors/clients (notably KWin) use `zwp_relative_pointer_v1` to receive high-resolution
 *   relative mouse deltas even when the "absolute" pointer position is also available.
 * - Trierarch’s Android input layer can operate in a relative (touchpad) mode. In that mode we emit
 *   both wl_pointer motion (absolute cursor position) and relative motion deltas to satisfy clients.
 *
 * Contract:
 * - This module only tracks per-client relative-pointer resources.
 * - Actual motion delivery happens in `pointer_input.c` via `send_relative_motion(...)`.
 */
#include "server_internal.h"
#include "relative-pointer-unstable-v1-server-protocol.h"
#include <stdlib.h>

static void relative_pointer_destroy(struct wl_client *c, struct wl_resource *r) {
    (void)c; wl_resource_destroy(r);
}
static const struct zwp_relative_pointer_v1_interface relative_pointer_impl = {
    .destroy = relative_pointer_destroy,
};

static void relative_pointer_manager_destroy(struct wl_client *c, struct wl_resource *r) {
    (void)c; wl_resource_destroy(r);
}
static void relative_pointer_manager_get_relative_pointer(struct wl_client *client,
        struct wl_resource *resource, uint32_t id, struct wl_resource *pointer) {
    (void)pointer;
    struct wayland_server *srv = wl_resource_get_user_data(resource);
    struct wl_resource *rp = wl_resource_create(client,
            &zwp_relative_pointer_v1_interface, 1, id);
    if (!rp) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(rp, &relative_pointer_impl, NULL, NULL);
    if (srv)
        track_input_resource(&srv->relative_pointer_resources, rp);
}

static const struct zwp_relative_pointer_manager_v1_interface relative_pointer_manager_impl = {
    .destroy = relative_pointer_manager_destroy,
    .get_relative_pointer = relative_pointer_manager_get_relative_pointer,
};

void relative_pointer_manager_bind(struct wl_client *client, void *data,
        uint32_t version, uint32_t id) {
    (void)version;
    struct wl_resource *res = wl_resource_create(client,
            &zwp_relative_pointer_manager_v1_interface, 1, id);
    if (!res) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(res, &relative_pointer_manager_impl, data, NULL);
}
