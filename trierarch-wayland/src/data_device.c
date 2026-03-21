/*
 * wl_data_device_manager + wl_data_device: selection (clipboard) and minimal dnd.
 * Selection: track source, broadcast selection(offer), forward offer.receive to source send.
 */
#include "server_internal.h"
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Mime type node for data_source */
struct data_source_mime {
    char *mime;
    struct wl_list link;
};

struct data_source_info {
    struct wl_list mime_types;
};

struct selection_cleanup {
    struct wl_listener listener;
    struct wayland_server *srv;
};
static struct selection_cleanup selection_cleanup;

static void selection_source_destroy_notify(struct wl_listener *listener, void *data) {
    (void)data;
    struct selection_cleanup *c = wl_container_of(listener, c, listener);
    if (c->srv) c->srv->selection_source = NULL;
}

static void data_device_release(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void data_device_start_drag(struct wl_client *client, struct wl_resource *resource,
        struct wl_resource *source, struct wl_resource *origin, struct wl_resource *icon, uint32_t serial) {
    (void)client; (void)resource; (void)source; (void)origin; (void)icon; (void)serial;
}

static void data_offer_accept(struct wl_client *client, struct wl_resource *resource,
        uint32_t serial, const char *mime_type) {
    (void)client; (void)resource; (void)serial; (void)mime_type;
}

static void data_offer_receive(struct wl_client *client, struct wl_resource *resource,
        const char *mime_type, int32_t fd) {
    (void)client;
    struct wl_resource *source = (struct wl_resource *)wl_resource_get_user_data(resource);
    if (source && wl_resource_get_version(source) >= 1)
        wl_data_source_send_send(source, mime_type, fd);
    else if (fd >= 0)
        close(fd);
}

static void data_offer_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void data_offer_finish(struct wl_client *client, struct wl_resource *resource) {
    (void)client; (void)resource;
}

static void data_offer_set_actions(struct wl_client *client, struct wl_resource *resource,
        uint32_t dnd_actions, uint32_t preferred_action) {
    (void)client; (void)resource; (void)dnd_actions; (void)preferred_action;
}

static const struct wl_data_offer_interface data_offer_impl = {
    .accept = data_offer_accept,
    .receive = data_offer_receive,
    .destroy = data_offer_destroy,
    .finish = data_offer_finish,
    .set_actions = data_offer_set_actions,
};

static void data_device_set_selection(struct wl_client *client, struct wl_resource *resource,
        struct wl_resource *source, uint32_t serial) {
    (void)client; (void)serial;
    struct wayland_server *srv = (struct wayland_server *)wl_resource_get_user_data(resource);
    if (!srv) return;
    if (srv->selection_source) {
        wl_list_remove(&selection_cleanup.listener.link);
        wl_list_init(&selection_cleanup.listener.link);
        srv->selection_source = NULL;
    }
    if (!source) {
        /* Clear selection */
        struct input_resource_node *node;
        wl_list_for_each(node, &srv->data_device_resources, link) {
            wl_data_device_send_selection(node->resource, NULL);
        }
        return;
    }
    selection_cleanup.srv = srv;
    selection_cleanup.listener.notify = selection_source_destroy_notify;
    wl_resource_add_destroy_listener(source, &selection_cleanup.listener);
    srv->selection_source = source;

    struct data_source_info *info = (struct data_source_info *)wl_resource_get_user_data(source);
    if (!info || wl_list_empty(&info->mime_types)) {
        struct input_resource_node *node;
        wl_list_for_each(node, &srv->data_device_resources, link) {
            struct wl_resource *offer = wl_resource_create(wl_resource_get_client(node->resource),
                    &wl_data_offer_interface, wl_resource_get_version(node->resource), 0);
            if (offer) {
                wl_resource_set_implementation(offer, &data_offer_impl, source, NULL);
                wl_data_offer_send_offer(offer, "text/plain");
                wl_data_device_send_selection(node->resource, offer);
            }
        }
        return;
    }

    struct data_source_mime *mime_node;
    struct input_resource_node *node;
    wl_list_for_each(node, &srv->data_device_resources, link) {
        struct wl_resource *offer = wl_resource_create(wl_resource_get_client(node->resource),
                &wl_data_offer_interface, wl_resource_get_version(node->resource), 0);
        if (!offer) continue;
        wl_resource_set_implementation(offer, &data_offer_impl, source, NULL);
        wl_list_for_each(mime_node, &info->mime_types, link) {
            wl_data_offer_send_offer(offer, mime_node->mime);
        }
        wl_data_device_send_selection(node->resource, offer);
    }
}

static const struct wl_data_device_interface data_device_impl = {
    .release = data_device_release,
    .start_drag = data_device_start_drag,
    .set_selection = data_device_set_selection,
};

static void manager_get_data_device(struct wl_client *client, struct wl_resource *manager_res,
        uint32_t id, struct wl_resource *seat_res) {
    (void)seat_res;
    struct wayland_server *srv = (struct wayland_server *)wl_resource_get_user_data(manager_res);
    if (!srv) return;
    uint32_t version = wl_resource_get_version(manager_res);
    struct wl_resource *res = wl_resource_create(client, &wl_data_device_interface, version, id);
    if (!res) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(res, &data_device_impl, srv, NULL);
    track_input_resource(&srv->data_device_resources, res);

    if (srv->selection_source) {
        struct wl_resource *offer = wl_resource_create(client, &wl_data_offer_interface, version, 0);
        if (offer) {
            struct data_source_info *info = (struct data_source_info *)wl_resource_get_user_data(srv->selection_source);
            wl_resource_set_implementation(offer, &data_offer_impl, srv->selection_source, NULL);
            if (info && !wl_list_empty(&info->mime_types)) {
                struct data_source_mime *mime_node;
                wl_list_for_each(mime_node, &info->mime_types, link)
                    wl_data_offer_send_offer(offer, mime_node->mime);
            } else {
                wl_data_offer_send_offer(offer, "text/plain");
            }
            wl_data_device_send_selection(res, offer);
        }
    }
}

static void data_source_offer(struct wl_client *client, struct wl_resource *resource, const char *mime_type) {
    (void)client;
    struct data_source_info *info = (struct data_source_info *)wl_resource_get_user_data(resource);
    if (!info) return;
    struct data_source_mime *m = (struct data_source_mime *)malloc(sizeof(*m));
    if (!m) {
        wl_client_post_no_memory(wl_resource_get_client(resource));
        return;
    }
    m->mime = strdup(mime_type ? mime_type : "");
    if (!m->mime) {
        free(m);
        wl_client_post_no_memory(wl_resource_get_client(resource));
        return;
    }
    wl_list_insert(info->mime_types.prev, &m->link);
}

static void data_source_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    struct data_source_info *info = (struct data_source_info *)wl_resource_get_user_data(resource);
    if (info) {
        struct data_source_mime *m, *tmp;
        wl_list_for_each_safe(m, tmp, &info->mime_types, link) {
            wl_list_remove(&m->link);
            free(m->mime);
            free(m);
        }
        free(info);
    }
    wl_resource_destroy(resource);
}

static void data_source_set_actions(struct wl_client *client, struct wl_resource *resource, uint32_t dnd_actions) {
    (void)client; (void)resource; (void)dnd_actions;
}

static const struct wl_data_source_interface data_source_impl = {
    .offer = data_source_offer,
    .destroy = data_source_destroy,
    .set_actions = data_source_set_actions,
};

static void manager_create_data_source(struct wl_client *client, struct wl_resource *manager_res, uint32_t id) {
    struct wayland_server *srv = (struct wayland_server *)wl_resource_get_user_data(manager_res);
    uint32_t version = wl_resource_get_version(manager_res);
    struct wl_resource *res = wl_resource_create(client, &wl_data_source_interface, version, id);
    if (!res) {
        wl_client_post_no_memory(client);
        return;
    }
    struct data_source_info *info = (struct data_source_info *)calloc(1, sizeof(*info));
    if (!info) {
        wl_resource_destroy(res);
        wl_client_post_no_memory(client);
        return;
    }
    wl_list_init(&info->mime_types);
    wl_resource_set_implementation(res, &data_source_impl, info, NULL);
}

static const struct wl_data_device_manager_interface manager_impl = {
    .create_data_source = manager_create_data_source,
    .get_data_device = manager_get_data_device,
};

void data_device_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    struct wl_resource *res = wl_resource_create(client, &wl_data_device_manager_interface, version, id);
    if (!res) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(res, &manager_impl, data, NULL);
}
