/*
 * wl_seat: bind and get_pointer/get_keyboard/get_touch. pointer_set_cursor stores cursor for renderer.
 */
#include "server_internal.h"
#include <stdlib.h>

static void pointer_set_cursor(struct wl_client *client, struct wl_resource *resource,
        uint32_t serial, struct wl_resource *surface_res, int32_t hotspot_x, int32_t hotspot_y) {
    (void)client;
    (void)serial;
    struct wayland_server *srv = wl_resource_get_user_data(resource);
    if (!srv) return;
    if (srv->cursor_surface) {
        srv->cursor_surface->is_cursor = false;
        srv->cursor_surface = NULL;
    }
    if (surface_res) {
        struct compositor_surface *surf = wl_resource_get_user_data(surface_res);
        if (surf) {
            surf->is_cursor = true;
            srv->cursor_surface = surf;
            srv->cursor_hotspot_x = hotspot_x;
            srv->cursor_hotspot_y = hotspot_y;
        }
    }
}
static void pointer_release(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}
static const struct wl_pointer_interface pointer_impl = {
    .set_cursor = pointer_set_cursor,
    .release = pointer_release,
};

static void keyboard_release(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}
static const struct wl_keyboard_interface keyboard_impl = {
    .release = keyboard_release,
};

static void touch_release(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}
static const struct wl_touch_interface touch_impl = {
    .release = touch_release,
};

static void seat_get_pointer(struct wl_client *client, struct wl_resource *seat_res, uint32_t id) {
    struct wayland_server *srv = wl_resource_get_user_data(seat_res);
    struct wl_resource *res = wl_resource_create(client, &wl_pointer_interface,
            wl_resource_get_version(seat_res), id);
    if (!res) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(res, &pointer_impl, srv, NULL);
    if (srv)
        track_input_resource(&srv->pointer_resources, res);
}
static void seat_get_keyboard(struct wl_client *client, struct wl_resource *seat_res, uint32_t id) {
    struct wayland_server *srv = wl_resource_get_user_data(seat_res);
    struct wl_resource *res = wl_resource_create(client, &wl_keyboard_interface,
            wl_resource_get_version(seat_res), id);
    if (res) {
        wl_resource_set_implementation(res, &keyboard_impl, srv, NULL);
        if (srv)
            track_input_resource(&srv->keyboard_resources, res);
    } else {
        wl_client_post_no_memory(client);
    }
}
static void seat_get_touch(struct wl_client *client, struct wl_resource *seat_res, uint32_t id) {
    struct wl_resource *res = wl_resource_create(client, &wl_touch_interface,
            wl_resource_get_version(seat_res), id);
    if (res)
        wl_resource_set_implementation(res, &touch_impl, NULL, NULL);
    else
        wl_client_post_no_memory(client);
}
static void seat_release(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct wl_seat_interface seat_impl = {
    .get_pointer = seat_get_pointer,
    .get_keyboard = seat_get_keyboard,
    .get_touch = seat_get_touch,
    .release = seat_release,
};

void seat_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct wl_resource *resource = wl_resource_create(client, &wl_seat_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &seat_impl, data, NULL);
    wl_seat_send_capabilities(resource, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_TOUCH);
    wl_seat_send_name(resource, "seat0");
}
