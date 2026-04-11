/* EGL/GLES2 renderer: composite Wayland surfaces onto an Android Surface. */
#ifndef TRIERARCH_RENDERER_H
#define TRIERARCH_RENDERER_H

#include <android/native_window.h>
#include <stdbool.h>

struct wayland_server;

typedef struct renderer_context renderer_context_t;

/**
 * @param skip_egl_wl_bind If non-zero, skip eglBindWaylandDisplayWL (no EGL_WAYLAND_BUFFER_WL import).
 * Leave zero unless every client only attaches linux-dmabuf buffers: when skipped, Mesa EGL
 * wl_buffers with no user_data are dropped (see surface.c). Default path keeps import enabled;
 * dmabuf may still fall back to mmap in renderer.c if host EGL rejects the guest buffer.
 */
renderer_context_t *renderer_create(ANativeWindow *window, struct wayland_server *srv, int skip_egl_wl_bind);
void renderer_destroy(renderer_context_t *ctx);
void renderer_release_context(renderer_context_t *ctx);
bool renderer_is_valid(renderer_context_t *ctx);
void renderer_get_size(renderer_context_t *ctx, int *w, int *h);
bool renderer_render(renderer_context_t *ctx, struct wayland_server *srv);

#endif
