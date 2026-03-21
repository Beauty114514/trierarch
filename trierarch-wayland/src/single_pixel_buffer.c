/*
 * wp_single_pixel_buffer_manager_v1: create 1x1 solid-color wl_buffer.
 * Required by KDE/KWin for decorations and backgrounds.
 */
#include "server_internal.h"
#include "single-pixel-buffer-v1-server-protocol.h"
#include <stdlib.h>

static void sp_manager_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void sp_manager_create_u32_rgba_buffer(struct wl_client *client,
        struct wl_resource *resource, uint32_t id,
        uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    (void)resource;
    uint8_t rr = (uint8_t)(r >> 24);
    uint8_t gg = (uint8_t)(g >> 24);
    uint8_t bb = (uint8_t)(b >> 24);
    uint8_t aa = (uint8_t)(a >> 24);
    uint32_t pixel = ((uint32_t)aa << 24) | ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | bb;

    struct wl_resource *buf_res = buffer_create_single_pixel(client, id, pixel);
    if (!buf_res)
        wl_client_post_no_memory(client);
}

static const struct wp_single_pixel_buffer_manager_v1_interface sp_manager_impl = {
    .destroy = sp_manager_destroy,
    .create_u32_rgba_buffer = sp_manager_create_u32_rgba_buffer,
};

void single_pixel_buffer_bind(struct wl_client *client, void *data,
        uint32_t version, uint32_t id) {
    (void)version;
    struct wl_resource *res = wl_resource_create(client,
            &wp_single_pixel_buffer_manager_v1_interface, 1, id);
    if (!res) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(res, &sp_manager_impl, data, NULL);
}
