/*
 * Public API for the Trierarch Wayland compositor.
 *
 * This header is consumed by `jni_bridge.c` and the renderer. Keep it focused on **contract**:
 * - lifecycle (create/destroy)
 * - how dispatch is driven (render thread vs background dispatch thread)
 * - what "has client" means (desktop connected) for Display-script idempotency
 *
 * Do not document every implementation detail here; prefer `server_internal.h` for internals.
 */
#ifndef TRIERARCH_COMPOSITOR_H
#define TRIERARCH_COMPOSITOR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct wayland_server wayland_server_t;
struct dmabuf_buffer;

/* Snapshot of a surface for the renderer.
 * - pixels: wl_shm
 * - egl_buffer: EGL client buffer (wl_resource*) imported via EGL_WAYLAND_BUFFER_WL
 * - dmabuf: linux-dmabuf
 * - ahb: Android HardwareBuffer (AHardwareBuffer*) imported via EGL_NATIVE_BUFFER_ANDROID
 */
typedef struct compositor_surface_view {
    const void *pixels;
    void *egl_buffer;  /* wl_resource* when from EGL client buffer; NULL for SHM */
    struct dmabuf_buffer *dmabuf; /* non-NULL: import fd in renderer (EGL dma-buf) */
    void *ahb; /* AHardwareBuffer* (opaque here to avoid NDK headers in public API) */
    int32_t width, height, stride;
    uint32_t format;
    int32_t x, y;
    int32_t buf_width, buf_height;
    float src_x, src_y, src_w, src_h;
    bool position_in_physical;  /* if true, x,y are already in physical pixels (e.g. cursor) */
} compositor_surface_view_t;

/* Lifecycle */
wayland_server_t *compositor_create(const char *runtime_dir);
/* Create a server with a specific socket name under XDG_RUNTIME_DIR. */
wayland_server_t *compositor_create_named(const char *runtime_dir, const char *socket_name);
void compositor_destroy(wayland_server_t *srv);

/* Event dispatch (non-blocking from render thread, or blocking from background thread) */
void compositor_dispatch(wayland_server_t *srv);
void compositor_dispatch_timeout(wayland_server_t *srv, int timeout_ms);

/* Surface iteration for rendering */
int compositor_get_surface_count(wayland_server_t *srv);
/* True if any xdg_toplevel (desktop/app) is connected; used to avoid re-running Display startup script. */
bool compositor_has_toplevel_client(wayland_server_t *srv);
void compositor_foreach_surface(wayland_server_t *srv,
    int (*callback)(const compositor_surface_view_t *view, void *user), void *user);
void compositor_send_frame_callbacks(wayland_server_t *srv);
/* Send xdg_wm_base ping to all bound clients (throttled ~1s); call from dispatch/render loop. */
void compositor_send_ping_to_clients(wayland_server_t *srv);

/* Output size/scale */
void compositor_set_output_size(wayland_server_t *srv,
    int32_t logical_w, int32_t logical_h, int32_t physical_w, int32_t physical_h);
void compositor_set_output_override(wayland_server_t *srv, int32_t w, int32_t h);
void compositor_get_output_size(wayland_server_t *srv, int32_t *w, int32_t *h);
int32_t compositor_get_output_scale(wayland_server_t *srv);
void compositor_set_output_user_scale(wayland_server_t *srv, int32_t scale);

/* Cursor: set position in physical/surface pixels (call from input when delivering pointer). */
void compositor_set_cursor_physical(wayland_server_t *srv, float x, float y);
/* Cursor: get view for drawing at pointer position (from wl_pointer.set_cursor). */
bool compositor_get_cursor_view(wayland_server_t *srv, compositor_surface_view_t *out);
/* Cursor: show for relative input, hide for absolute input (e.g. touch = finger as cursor). */
void compositor_set_cursor_visible(wayland_server_t *srv, bool visible);

/* For EGL: get wl_display for eglBindWaylandDisplayWL */
void *compositor_get_wl_display(wayland_server_t *srv);
/* Enable EGL buffer attach (call after eglBindWaylandDisplayWL succeeds) */
void compositor_set_egl_buffer_supported(wayland_server_t *srv, bool supported);

/* WM mode — controls configure, rendering order, and input focus strategy.
 *
 *   WM_MODE_NESTED (default, 0): hand full output to a nested compositor (KDE, Sway, …).
 *   WM_MODE_DIRECT         (1): manage individual app windows ourselves.
 *
 * Set before the first client connects; changing mid-session is safe but may confuse
 * already-connected clients that have received configure events under the previous mode.
 */
typedef enum { WM_MODE_NESTED = 0, WM_MODE_DIRECT = 1 } wm_mode_t;
void compositor_set_wm_mode(wayland_server_t *srv, wm_mode_t mode);

#endif
