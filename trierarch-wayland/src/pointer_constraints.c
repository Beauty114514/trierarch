/*
 * zwp_pointer_constraints_v1: pointer lock/confine. Required by KWin.
 */
#include "server_internal.h"
#include "pointer-constraints-unstable-v1-server-protocol.h"
#include <stdlib.h>

static void pointer_constraints_destroy(struct wl_client *c, struct wl_resource *r) {
    (void)c; wl_resource_destroy(r);
}

static void locked_pointer_destroy(struct wl_client *c, struct wl_resource *r) {
    (void)c; wl_resource_destroy(r);
}
static void locked_pointer_set_cursor_position_hint(struct wl_client *c,
        struct wl_resource *r, wl_fixed_t x, wl_fixed_t y) {
    (void)c; (void)r; (void)x; (void)y;
}
static void locked_pointer_set_region(struct wl_client *c,
        struct wl_resource *r, struct wl_resource *region) {
    (void)c; (void)r; (void)region;
}
static const struct zwp_locked_pointer_v1_interface locked_pointer_impl = {
    .destroy = locked_pointer_destroy,
    .set_cursor_position_hint = locked_pointer_set_cursor_position_hint,
    .set_region = locked_pointer_set_region,
};

static void confine_pointer_destroy(struct wl_client *c, struct wl_resource *r) {
    (void)c; wl_resource_destroy(r);
}
static void confine_pointer_set_region(struct wl_client *c,
        struct wl_resource *r, struct wl_resource *region) {
    (void)c; (void)r; (void)region;
}
static const struct zwp_confined_pointer_v1_interface confined_pointer_impl = {
    .destroy = confine_pointer_destroy,
    .set_region = confine_pointer_set_region,
};

static void pointer_constraints_lock_pointer(struct wl_client *client,
        struct wl_resource *pc_res, uint32_t id, struct wl_resource *surface,
        struct wl_resource *pointer, struct wl_resource *region, uint32_t lifetime) {
    (void)pc_res; (void)surface; (void)pointer; (void)region; (void)lifetime;
    struct wl_resource *lp = wl_resource_create(client, &zwp_locked_pointer_v1_interface, 1, id);
    if (!lp) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(lp, &locked_pointer_impl, NULL, NULL);
    zwp_locked_pointer_v1_send_locked(lp);
}

static void pointer_constraints_confine_pointer(struct wl_client *client,
        struct wl_resource *pc_res, uint32_t id, struct wl_resource *surface,
        struct wl_resource *pointer, struct wl_resource *region, uint32_t lifetime) {
    (void)pc_res; (void)surface; (void)pointer; (void)region; (void)lifetime;
    struct wl_resource *cp = wl_resource_create(client, &zwp_confined_pointer_v1_interface, 1, id);
    if (!cp) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(cp, &confined_pointer_impl, NULL, NULL);
    zwp_confined_pointer_v1_send_confined(cp);
}

static const struct zwp_pointer_constraints_v1_interface pointer_constraints_impl = {
    .destroy = pointer_constraints_destroy,
    .lock_pointer = pointer_constraints_lock_pointer,
    .confine_pointer = pointer_constraints_confine_pointer,
};

void pointer_constraints_bind(struct wl_client *client, void *data,
        uint32_t version, uint32_t id) {
    (void)data; (void)version;
    struct wl_resource *res = wl_resource_create(client, &zwp_pointer_constraints_v1_interface, 1, id);
    if (!res) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(res, &pointer_constraints_impl, NULL, NULL);
}
