/*
 * wl_shm and wl_buffer: SHM and EGL buffer management. Pools, buffer creation/destroy.
 */
#include "server_internal.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "TrierarchBuffer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/* Set by compositor.c so buffer_resource_destroy can clear surface refs */
extern struct wayland_server *g_wayland_server;

static void shm_pool_unref(struct shm_pool *pool) {
    if (!pool) return;
    pool->refcount--;
    if (pool->refcount <= 0) {
        if (pool->data && pool->data != MAP_FAILED)
            munmap(pool->data, pool->size);
        free(pool);
    }
}

static void shm_buffer_release(struct shm_buffer *buf) {
    if (!buf) return;
    if (buf->resource)
        wl_resource_post_event(buf->resource, WL_BUFFER_RELEASE);
    buf->owner = NULL;
}

static void *shm_buffer_get_data(struct shm_buffer *buf) {
    if (!buf || !buf->pool || !buf->pool->data) return NULL;
    return (char *)buf->pool->data + buf->offset;
}

static void buffer_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct wl_buffer_interface buffer_impl = { .destroy = buffer_destroy };

static void buffer_resource_destroy(struct wl_resource *resource) {
    struct shm_buffer *buf = wl_resource_get_user_data(resource);
    wl_resource_set_user_data(resource, NULL);
    if (!buf) return;

    if (g_wayland_server) {
        pthread_mutex_lock(&g_wayland_server->surfaces_mutex);
        struct compositor_surface *surf;
        wl_list_for_each(surf, &g_wayland_server->surfaces, link) {
            if (surf->current_buffer && surf->current_buffer->type == BUF_SHM && surf->current_buffer->u.shm == buf)
                surf->current_buffer->u.shm = NULL;
            if (surf->pending_buffer && surf->pending_buffer->type == BUF_SHM && surf->pending_buffer->u.shm == buf)
                surf->pending_buffer->u.shm = NULL;
        }
        pthread_mutex_unlock(&g_wayland_server->surfaces_mutex);
    }
    buf->resource = NULL;
    shm_pool_unref(buf->pool);
    free(buf);
}

static void egl_buffer_resource_destroyed(struct wl_listener *listener, void *data) {
    (void)data;
    struct egl_buffer_ref *egl = wl_container_of(listener, egl, resource_listener);
    wl_list_remove(&egl->resource_listener.link);
    egl->resource = NULL;
    struct compositor_surface *surf = egl->surf;
    struct compositor_buffer_ref *ref = egl->ref;
    egl->surf = NULL;
    egl->ref = NULL;
    if (surf && ref) {
        if (surf->current_buffer == ref) surf->current_buffer = NULL;
        if (surf->pending_buffer == ref) surf->pending_buffer = NULL;
    }
    free(egl);
    free(ref);
}

static void pool_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    struct shm_pool *pool = wl_resource_get_user_data(resource);
    wl_resource_set_user_data(resource, NULL);
    shm_pool_unref(pool);
    wl_resource_destroy(resource);
}

static void pool_create_buffer(struct wl_client *client, struct wl_resource *pool_res,
        uint32_t id, int32_t offset, int32_t width, int32_t height, int32_t stride, uint32_t format) {
    struct shm_pool *pool = wl_resource_get_user_data(pool_res);
    if (!pool || width <= 0 || height <= 0 || stride < width * 4 ||
            (size_t)(offset + (int64_t)stride * height) > pool->size) {
        wl_resource_post_error(pool_res, WL_SHM_ERROR_INVALID_STRIDE, "invalid buffer params");
        return;
    }
    if (format != WL_SHM_FORMAT_ARGB8888 && format != WL_SHM_FORMAT_XRGB8888) {
        wl_resource_post_error(pool_res, WL_SHM_ERROR_INVALID_FORMAT, "unsupported format");
        return;
    }

    struct shm_buffer *buf = calloc(1, sizeof(*buf));
    if (!buf) {
        wl_client_post_no_memory(client);
        return;
    }
    buf->resource = NULL;
    buf->pool = pool;
    buf->offset = offset;
    buf->width = width;
    buf->height = height;
    buf->stride = stride;
    buf->format = format;
    buf->owner = NULL;
    pool->refcount++;

    struct wl_resource *buf_res = wl_resource_create(client, &wl_buffer_interface, 1, id);
    if (!buf_res) {
        shm_pool_unref(buf->pool);
        free(buf);
        wl_client_post_no_memory(client);
        return;
    }
    buf->resource = buf_res;
    wl_resource_set_implementation(buf_res, &buffer_impl, buf, buffer_resource_destroy);
}

static void pool_resize(struct wl_client *client, struct wl_resource *resource, int32_t size) {
    (void)client;
    struct shm_pool *pool = wl_resource_get_user_data(resource);
    if (!pool) return;
    if ((size_t)size < pool->size) {
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "shrinking pool invalid");
        return;
    }
    if ((size_t)size == pool->size) return;
#ifdef MREMAP_MAYMOVE
    void *new_data = mremap(pool->data, pool->size, (size_t)size, MREMAP_MAYMOVE);
    if (new_data != MAP_FAILED) {
        pool->data = new_data;
        pool->size = (size_t)size;
    } else {
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "mremap failed");
    }
#else
    (void)size;
    wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "pool resize not supported");
#endif
}

static const struct wl_shm_pool_interface pool_impl = {
    .create_buffer = pool_create_buffer,
    .destroy = pool_destroy,
    .resize = pool_resize,
};

static void pool_resource_destroy(struct wl_resource *resource) {
    struct shm_pool *pool = wl_resource_get_user_data(resource);
    if (pool) shm_pool_unref(pool);
}

static void shm_create_pool(struct wl_client *client, struct wl_resource *resource,
        uint32_t id, int fd, int32_t size) {
    if (size <= 0) {
        close(fd);
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_STRIDE, "invalid size");
        return;
    }
    struct shm_pool *pool = calloc(1, sizeof(*pool));
    if (!pool) {
        close(fd);
        wl_client_post_no_memory(client);
        return;
    }
    pool->data = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (pool->data == MAP_FAILED) {
        wl_resource_post_error(resource, WL_SHM_ERROR_INVALID_FD, "mmap failed");
        free(pool);
        return;
    }
    pool->size = (size_t)size;
    pool->refcount = 1;

    struct wl_resource *pool_res = wl_resource_create(client, &wl_shm_pool_interface,
            wl_resource_get_version(resource), id);
    if (!pool_res) {
        munmap(pool->data, pool->size);
        free(pool);
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(pool_res, &pool_impl, pool, pool_resource_destroy);
}

static const struct wl_shm_interface shm_impl = {
    .create_pool = shm_create_pool,
};

void buffer_shm_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    (void)data;
    struct wl_resource *resource = wl_resource_create(client, &wl_shm_interface, version, id);
    if (!resource) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(resource, &shm_impl, NULL, NULL);
    /* Advertise both common formats. Some EGL/Wayland clients fail to pick an EGLConfig
     * if ARGB8888 is not advertised. We handle "alpha=0 makes surface invisible" in the
     * renderer by forcing SHM buffers to be treated as opaque when needed. */
    wl_shm_send_format(resource, WL_SHM_FORMAT_ARGB8888);
    wl_shm_send_format(resource, WL_SHM_FORMAT_XRGB8888);
    {
        static unsigned g_bind_logged;
        if (g_bind_logged < 128) {
            pid_t pid = (pid_t)-1;
            uid_t uid = 0;
            gid_t gid = 0;
            wl_client_get_credentials(client, &pid, &uid, &gid);
            /* wl_shm is a strong indicator the client can fall back to SHM rendering. */
            LOGI("bind wl_shm v%u pid=%d uid=%u (n=%u)",
                    version, (int)pid, (unsigned)uid, g_bind_logged + 1);
            g_bind_logged++;
        }
    }
}

static void egl_buffer_release(struct egl_buffer_ref *egl) {
    if (!egl) return;
    if (egl->resource) {
        wl_list_remove(&egl->resource_listener.link);
        wl_resource_post_event(egl->resource, WL_BUFFER_RELEASE);
        wl_resource_set_user_data(egl->resource, NULL);
        egl->resource = NULL;
    }
    egl->surf = NULL;
    egl->ref = NULL;
    free(egl);
}

static void dmabuf_buffer_release(struct dmabuf_buffer *db) {
    if (!db) return;
    if (db->resource)
        wl_resource_post_event(db->resource, WL_BUFFER_RELEASE);
    db->owner = NULL;
}

static void ahb_buffer_release(struct ahb_buffer *ab) {
    if (!ab) return;
    if (ab->resource)
        wl_resource_post_event(ab->resource, WL_BUFFER_RELEASE);
    ab->owner = NULL;
}

void buffer_ref_release(struct compositor_buffer_ref *ref) {
    if (!ref) return;
    if (ref->type == BUF_SHM && ref->u.shm)
        shm_buffer_release(ref->u.shm);
    else if (ref->type == BUF_EGL && ref->u.egl)
        egl_buffer_release(ref->u.egl);
    else if (ref->type == BUF_DMABUF && ref->u.dmabuf)
        dmabuf_buffer_release(ref->u.dmabuf);
    else if (ref->type == BUF_AHB && ref->u.ahb)
        ahb_buffer_release(ref->u.ahb);
    free(ref);
}

void buffer_ref_release_no_post(struct compositor_buffer_ref *ref) {
    if (!ref) return;
    if (ref->type == BUF_SHM && ref->u.shm)
        ref->u.shm->owner = NULL;
    else if (ref->type == BUF_EGL && ref->u.egl) {
        struct egl_buffer_ref *egl = ref->u.egl;
        if (egl->resource) {
            wl_list_remove(&egl->resource_listener.link);
            wl_resource_set_user_data(egl->resource, NULL);
            egl->resource = NULL;
        }
        egl->surf = NULL;
        egl->ref = NULL;
        free(egl);
    } else if (ref->type == BUF_DMABUF && ref->u.dmabuf) {
        ref->u.dmabuf->owner = NULL;
    } else if (ref->type == BUF_AHB && ref->u.ahb) {
        ref->u.ahb->owner = NULL;
    }
    free(ref);
}

void *buffer_ref_get_data(struct compositor_buffer_ref *ref) {
    if (!ref) return NULL;
    if (ref->type == BUF_SHM && ref->u.shm) return shm_buffer_get_data(ref->u.shm);
    return NULL;
}

int32_t buffer_ref_width(struct compositor_buffer_ref *ref) {
    if (!ref) return 0;
    if (ref->type == BUF_SHM && ref->u.shm) return ref->u.shm->width;
    if (ref->type == BUF_EGL && ref->u.egl) return ref->u.egl->width;
    if (ref->type == BUF_DMABUF && ref->u.dmabuf) return ref->u.dmabuf->width;
    if (ref->type == BUF_AHB && ref->u.ahb) return ref->u.ahb->width;
    return 0;
}

int32_t buffer_ref_height(struct compositor_buffer_ref *ref) {
    if (!ref) return 0;
    if (ref->type == BUF_SHM && ref->u.shm) return ref->u.shm->height;
    if (ref->type == BUF_EGL && ref->u.egl) return ref->u.egl->height;
    if (ref->type == BUF_DMABUF && ref->u.dmabuf) return ref->u.dmabuf->height;
    if (ref->type == BUF_AHB && ref->u.ahb) return ref->u.ahb->height;
    return 0;
}

int32_t buffer_ref_stride(struct compositor_buffer_ref *ref) {
    if (!ref) return 0;
    if (ref->type == BUF_SHM && ref->u.shm) return ref->u.shm->stride;
    if (ref->type == BUF_EGL && ref->u.egl) return ref->u.egl->width * 4;
    if (ref->type == BUF_DMABUF && ref->u.dmabuf) return (int32_t)ref->u.dmabuf->stride;
    if (ref->type == BUF_AHB && ref->u.ahb) return ref->u.ahb->stride;
    return 0;
}

uint32_t buffer_ref_format(struct compositor_buffer_ref *ref) {
    if (!ref) return 0;
    if (ref->type == BUF_SHM && ref->u.shm) return ref->u.shm->format;
    if (ref->type == BUF_EGL && ref->u.egl) return WL_SHM_FORMAT_XRGB8888;
    if (ref->type == BUF_DMABUF && ref->u.dmabuf) return WL_SHM_FORMAT_ARGB8888;
    if (ref->type == BUF_AHB && ref->u.ahb) return ref->u.ahb->format;
    return 0;
}

void buffer_ref_set_owner(struct compositor_buffer_ref *ref, struct compositor_surface *surf) {
    if (!ref) return;
    if (ref->type == BUF_SHM && ref->u.shm) ref->u.shm->owner = surf;
    if (ref->type == BUF_EGL && ref->u.egl) ref->u.egl->surf = surf;
    if (ref->type == BUF_DMABUF && ref->u.dmabuf) ref->u.dmabuf->owner = surf;
    if (ref->type == BUF_AHB && ref->u.ahb) ref->u.ahb->owner = surf;
}

void buffer_ref_clear_owner(struct compositor_buffer_ref *ref) {
    if (!ref) return;
    if (ref->type == BUF_SHM && ref->u.shm) ref->u.shm->owner = NULL;
    if (ref->type == BUF_EGL && ref->u.egl) ref->u.egl->surf = NULL;
    if (ref->type == BUF_DMABUF && ref->u.dmabuf) ref->u.dmabuf->owner = NULL;
    if (ref->type == BUF_AHB && ref->u.ahb) ref->u.ahb->owner = NULL;
}

struct wl_resource *buffer_ref_get_egl_resource(struct compositor_buffer_ref *ref) {
    if (!ref || ref->type != BUF_EGL || !ref->u.egl) return NULL;
    return ref->u.egl->resource;
}

struct dmabuf_buffer *buffer_ref_get_dmabuf(struct compositor_buffer_ref *ref) {
    if (!ref || ref->type != BUF_DMABUF || !ref->u.dmabuf) return NULL;
    return ref->u.dmabuf;
}

struct compositor_buffer_ref *buffer_attach_egl_buffer(struct wl_client *client,
        struct wl_resource *buffer_res, struct compositor_surface *surf, int32_t w, int32_t h) {
    (void)client;
    struct egl_buffer_ref *egl = calloc(1, sizeof(*egl));
    struct compositor_buffer_ref *ref = calloc(1, sizeof(*ref));
    if (!egl || !ref) {
        free(egl);
        free(ref);
        return NULL;
    }
    egl->resource = buffer_res;
    egl->width = w;
    egl->height = h;
    egl->surf = surf;
    egl->ref = ref;
    egl->resource_listener.notify = egl_buffer_resource_destroyed;
    wl_resource_add_destroy_listener(buffer_res, &egl->resource_listener);
    ref->type = BUF_EGL;
    ref->u.egl = egl;
    return ref;
}

struct wl_resource *buffer_create_single_pixel(struct wl_client *client, uint32_t id,
        uint32_t pixel_argb) {
    struct shm_pool *pool = calloc(1, sizeof(*pool));
    if (!pool) return NULL;
    pool->data = malloc(4);
    if (!pool->data) { free(pool); return NULL; }
    pool->size = 4;
    pool->refcount = 1;
    memcpy(pool->data, &pixel_argb, 4);

    struct shm_buffer *buf = calloc(1, sizeof(*buf));
    if (!buf) { free(pool->data); free(pool); return NULL; }
    buf->pool = pool;
    buf->offset = 0;
    buf->width = 1;
    buf->height = 1;
    buf->stride = 4;
    buf->format = WL_SHM_FORMAT_ARGB8888;
    buf->owner = NULL;

    struct wl_resource *buf_res = wl_resource_create(client, &wl_buffer_interface, 1, id);
    if (!buf_res) { free(pool->data); free(pool); free(buf); return NULL; }
    buf->resource = buf_res;
    wl_resource_set_implementation(buf_res, &buffer_impl, buf, buffer_resource_destroy);
    return buf_res;
}
