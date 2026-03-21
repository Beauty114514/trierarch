/*
 * wl_subcompositor + wl_subsurface. Required by KDE/KWin and most desktop environments.
 */
#include "server_internal.h"
#include <stdlib.h>
#include <android/log.h>

#define LOG_TAG "TrierarchSubcomp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static void subsurface_resource_destroy(struct wl_resource *resource) {
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    if (!surf) return;
    surf->subsurface_res = NULL;
    if (surf->parent) {
        surf->parent = NULL;
        wl_list_remove(&surf->sub_link);
        wl_list_init(&surf->sub_link);
    }
}

static void subsurface_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void subsurface_set_position(struct wl_client *client, struct wl_resource *resource,
        int32_t x, int32_t y) {
    (void)client;
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    if (surf) {
        surf->sub_x = x;
        surf->sub_y = y;
    }
}

static void subsurface_place_above(struct wl_client *client, struct wl_resource *resource,
        struct wl_resource *sibling) {
    (void)client;
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    struct compositor_surface *sib = wl_resource_get_user_data(sibling);
    if (surf && sib && surf->parent && surf->parent == sib->parent) {
        wl_list_remove(&surf->sub_link);
        wl_list_insert(sib->sub_link.next, &surf->sub_link);
    }
}

static void subsurface_place_below(struct wl_client *client, struct wl_resource *resource,
        struct wl_resource *sibling) {
    (void)client;
    struct compositor_surface *surf = wl_resource_get_user_data(resource);
    struct compositor_surface *sib = wl_resource_get_user_data(sibling);
    if (surf && sib && surf->parent && surf->parent == sib->parent) {
        wl_list_remove(&surf->sub_link);
        wl_list_insert(&sib->sub_link, &surf->sub_link);
    }
}

static void subsurface_set_sync(struct wl_client *client, struct wl_resource *resource) {
    (void)client; (void)resource;
}

static void subsurface_set_desync(struct wl_client *client, struct wl_resource *resource) {
    (void)client; (void)resource;
}

static const struct wl_subsurface_interface subsurface_impl = {
    .destroy = subsurface_destroy,
    .set_position = subsurface_set_position,
    .place_above = subsurface_place_above,
    .place_below = subsurface_place_below,
    .set_sync = subsurface_set_sync,
    .set_desync = subsurface_set_desync,
};

static void subcompositor_get_subsurface(struct wl_client *client,
        struct wl_resource *subcomp_res, uint32_t id,
        struct wl_resource *surface_res, struct wl_resource *parent_res) {
    struct compositor_surface *child = wl_resource_get_user_data(surface_res);
    struct compositor_surface *parent = wl_resource_get_user_data(parent_res);
    if (!child || !parent) {
        wl_resource_post_error(subcomp_res, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                "invalid surface");
        return;
    }
    if (child->parent) {
        wl_resource_post_error(subcomp_res, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                "surface already has role");
        return;
    }
    if (child == parent) {
        wl_resource_post_error(subcomp_res, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                "surface cannot be its own parent");
        return;
    }
    struct compositor_surface *p = parent;
    while (p) {
        if (p == child) {
            wl_resource_post_error(subcomp_res, WL_SUBCOMPOSITOR_ERROR_BAD_PARENT,
                    "parent is descendant of child");
            return;
        }
        p = p->parent;
    }

    struct wl_resource *sub_res = wl_resource_create(client, &wl_subsurface_interface,
            wl_resource_get_version(subcomp_res), id);
    if (!sub_res) {
        wl_client_post_no_memory(client);
        return;
    }
    child->parent = parent;
    child->sub_x = 0;
    child->sub_y = 0;
    child->subsurface_res = sub_res;
    wl_list_insert(&parent->children, &child->sub_link);
    wl_resource_set_implementation(sub_res, &subsurface_impl, child, subsurface_resource_destroy);
    LOGI("subsurface created: child=%p parent=%p", (void*)child, (void*)parent);
}

static void subcompositor_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct wl_subcompositor_interface subcompositor_impl = {
    .destroy = subcompositor_destroy,
    .get_subsurface = subcompositor_get_subsurface,
};

void subcompositor_bind(struct wl_client *client, void *data,
        uint32_t version, uint32_t id) {
    struct wl_resource *res = wl_resource_create(client, &wl_subcompositor_interface,
            version < 1 ? version : 1, id);
    if (!res) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(res, &subcompositor_impl, data, NULL);
}
