/*
 * wp_presentation (presentation-time): frame timing feedback.
 * KDE requires this to create surfaces properly.
 */
#include "server_internal.h"
#include "presentation-time-server-protocol.h"
#include <stdlib.h>
#include <time.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

static void presentation_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void presentation_feedback(struct wl_client *client, struct wl_resource *pres_res,
        struct wl_resource *surface, uint32_t callback_id) {
    if (!surface || !pres_res) return;
    struct wl_resource *fb = wl_resource_create(client, &wp_presentation_feedback_interface,
            wl_resource_get_version(pres_res), callback_id);
    if (!fb) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(fb, NULL, NULL, NULL);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t tv_sec_hi = (uint32_t)(ts.tv_sec >> 32);
    uint32_t tv_sec_lo = (uint32_t)ts.tv_sec;
    uint32_t tv_nsec = (uint32_t)ts.tv_nsec;
    wp_presentation_feedback_send_presented(fb, tv_sec_hi, tv_sec_lo, tv_nsec,
            16666667, 0, 0, 0);
}

static const struct wp_presentation_interface presentation_impl = {
    .destroy = presentation_destroy,
    .feedback = presentation_feedback,
};

void presentation_bind(struct wl_client *client, void *data,
        uint32_t version, uint32_t id) {
    (void)data;
    uint32_t ver = (version < 2) ? version : 2;
    struct wl_resource *res = wl_resource_create(client, &wp_presentation_interface, ver, id);
    if (!res) { wl_client_post_no_memory(client); return; }
    wl_resource_set_implementation(res, &presentation_impl, NULL, NULL);
    wp_presentation_send_clock_id(res, CLOCK_MONOTONIC);
}
