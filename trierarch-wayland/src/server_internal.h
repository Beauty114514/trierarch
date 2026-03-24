/* Shared internal types and cross-module declarations for the Wayland server. */
#ifndef TRIERARCH_SERVER_INTERNAL_H
#define TRIERARCH_SERVER_INTERNAL_H

#include <wayland-server.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "compositor.h"

/* --- SHM pool (buffer.c) --- */
struct shm_pool {
    void *data;
    size_t size;
    int refcount;
};

struct shm_buffer {
    struct wl_resource *resource;
    struct shm_pool *pool;
    int32_t offset;
    int32_t width, height, stride;
    uint32_t format;
    struct compositor_surface *owner;
};

/* EGL buffer: client-created wl_buffer without user_data; rendered via EGL_WAYLAND_BUFFER_WL */
struct egl_buffer_ref {
    struct wl_resource *resource;
    int32_t width, height;
    struct compositor_surface *surf;
    struct compositor_buffer_ref *ref;
    struct wl_listener resource_listener;
};

enum compositor_buffer_type { BUF_SHM, BUF_EGL };

struct compositor_buffer_ref {
    enum compositor_buffer_type type;
    union {
        struct shm_buffer *shm;
        struct egl_buffer_ref *egl;
    } u;
};

/* Pending frame callback (surface.c) */
struct pending_frame_cb {
    struct wl_list link;
    struct wl_resource *resource;
    struct wl_listener resource_listener;
};

struct compositor_surface {
    struct wl_list link;
    struct wl_resource *resource;
    struct wayland_server *srv;
    struct compositor_buffer_ref *current_buffer;
    struct compositor_buffer_ref *pending_buffer;
    struct wl_resource *xdg_surface_res;
    struct wl_resource *xdg_toplevel_res;
    int32_t buffer_offset_x, buffer_offset_y;
    int32_t buffer_scale;
    bool entered_output;
    struct wl_resource *viewport_res;  /* wp_viewport (viewporter.c) */
    /* Subsurface support (subcompositor.c) */
    struct compositor_surface *parent;
    struct wl_resource *subsurface_res;
    struct wl_list children;   /* list of child compositor_surface via sub_link */
    struct wl_list sub_link;   /* link into parent->children */
    int32_t sub_x, sub_y;
    bool is_cursor;  /* true = cursor surface from wl_pointer.set_cursor, drawn at pointer position */
};

/* Input: track wl_pointer / zwp_relative_pointer_v1 resources for event delivery */
struct input_resource_node {
    struct wl_list link;
    struct wl_resource *resource;
    struct wl_listener destroy_listener;
};

/* Opaque server struct; full definition in compositor.c */
struct wayland_server {
    struct wl_display *display;
    struct wl_event_loop *loop;
    char *runtime_dir;
    struct wl_list surfaces;
    struct wl_list pending_frame_callbacks;
    pthread_mutex_t surfaces_mutex;
    bool valid;
    int32_t output_width, output_height;
    int32_t output_scale;
    int32_t output_override_w, output_override_h;
    struct wl_list output_resources;  /* list of output_resource_node */
    bool egl_buffer_supported;
    /* Pointer input (touch → wl_pointer / relative_pointer) */
    struct wl_list pointer_resources;
    struct wl_list relative_pointer_resources;
    struct compositor_surface *pointer_focus;
    wl_fixed_t pointer_x, pointer_y;
    struct compositor_surface *cursor_surface;
    int32_t cursor_hotspot_x, cursor_hotspot_y;
    float cursor_phys_x, cursor_phys_y;  /* cursor position in physical/surface pixels for drawing */
    bool cursor_visible;  /* false = absolute input (no cursor), true = relative input (show cursor) */
    /* Keyboard input (key events to focused client, e.g. KWin for approach A) */
    struct wl_list keyboard_resources;
    struct compositor_surface *keyboard_focus;
    /* Current depressed modifier bitmask (used for wl_keyboard.modifiers events). */
    uint32_t keyboard_mods_depressed;
    /* Current locked modifier bitmask (CapsLock/NumLock/ScrollLock). */
    uint32_t keyboard_mods_locked;
    /* Per-side pressed state for modifiers (more robust than counters). */
    bool keyboard_lshift_down;
    bool keyboard_rshift_down;
    bool keyboard_lctrl_down;
    bool keyboard_rctrl_down;
    bool keyboard_lalt_down;
    bool keyboard_ralt_down;
    bool keyboard_lmeta_down;
    bool keyboard_rmeta_down;
    bool keyboard_capslock_down;
    bool keyboard_numlock_down;
    bool keyboard_scrolllock_down;
    bool keyboard_capslock_enabled;
    bool keyboard_numlock_enabled;
    bool keyboard_scrolllock_enabled;
    struct wl_list wm_base_resources;  /* xdg_wm_base resources for ping */
    /* Selection (clipboard): current owner and data_device list for selection(offer) */
    struct wl_list data_device_resources;
    struct wl_resource *selection_source;  /* wl_data_source that owns selection */
};

struct output_resource_node {
    struct wl_list link;
    struct wl_resource *resource;
    struct wl_listener destroy_listener;
};

/* xdg_wm_base resources: for periodic ping (liveness) */
struct wm_base_resource_node {
    struct wl_list link;
    struct wl_resource *resource;
    struct wl_listener destroy_listener;
};

/* surface.c */
void surface_compositor_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

/* pointer_input.c: track resources and deliver pointer events */
void track_input_resource(struct wl_list *list, struct wl_resource *res);
void compositor_pointer_event(wayland_server_t *srv, float x, float y, int action, uint32_t time_ms);
void compositor_pointer_axis_event(wayland_server_t *srv, uint32_t time_ms,
    float delta_x, float delta_y, uint32_t axis_source);
void compositor_pointer_right_click(wayland_server_t *srv, uint32_t time_ms, float x, float y);

/* keyboard_input.c: keyboard focus follows pointer; deliver key events to focused client */
void keyboard_focus_update(struct wayland_server *srv, struct compositor_surface *surf);
void compositor_keyboard_key_event(wayland_server_t *srv, uint32_t time_ms, uint32_t key_linux, uint32_t state);

/* output.c, seat.c, xdg_shell.c */
void output_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);
void seat_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);
void xdg_shell_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);
/* xdg_shell.c: notify toplevel when output/size changes (called from output.c) */
void send_toplevel_configure(struct compositor_surface *surf);

/* subcompositor.c */
void subcompositor_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

/* single_pixel_buffer.c */
void single_pixel_buffer_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

/* viewporter.c */
void viewporter_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

/* xdg_decoration.c */
void xdg_decoration_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

/* xdg_output.c */
void xdg_output_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

/* pointer_constraints.c */
void pointer_constraints_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

/* relative_pointer.c */
void relative_pointer_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

/* presentation.c */
void presentation_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

/* data_device.c */
void data_device_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);

/* buffer.c: wl_shm bind and buffer_ref helpers */
void buffer_shm_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);
void buffer_ref_release(struct compositor_buffer_ref *ref);
void buffer_ref_release_no_post(struct compositor_buffer_ref *ref);
void *buffer_ref_get_data(struct compositor_buffer_ref *ref);
int32_t buffer_ref_width(struct compositor_buffer_ref *ref);
int32_t buffer_ref_height(struct compositor_buffer_ref *ref);
int32_t buffer_ref_stride(struct compositor_buffer_ref *ref);
uint32_t buffer_ref_format(struct compositor_buffer_ref *ref);
void buffer_ref_set_owner(struct compositor_buffer_ref *ref, struct compositor_surface *surf);
void buffer_ref_clear_owner(struct compositor_buffer_ref *ref);
struct wl_resource *buffer_create_single_pixel(struct wl_client *client, uint32_t id,
        uint32_t pixel_argb);
/** EGL buffer: return wl_resource* for renderer; NULL if ref is SHM. */
struct wl_resource *buffer_ref_get_egl_resource(struct compositor_buffer_ref *ref);
/** Create compositor_buffer_ref for EGL buffer; caller sets surf->pending_buffer. */
struct compositor_buffer_ref *buffer_attach_egl_buffer(struct wl_client *client,
        struct wl_resource *buffer_res, struct compositor_surface *surf, int32_t w, int32_t h);

#endif
