/*
 * android_wlegl: Android native buffer sharing on Wayland.
 *
 * This is used by Android EGL wrapper libEGL to share GraphicBuffer/AHardwareBuffer-backed
 * buffers with a Wayland compositor (similar to Termux:X11's AHardwareBuffer->EGLImage path).
 *
 * We implement a minimal server-side android_wlegl interface:
 * - create_handle: creates a temporary handle object which collects fds + int payload
 * - create_buffer: wraps the collected native_handle as an AHardwareBuffer and exposes it as wl_buffer
 *
 * The resulting wl_buffer gets wl_resource user_data = struct ahb_buffer, so wl_surface.attach
 * can detect BUF_AHB and the renderer can import it via EGL_NATIVE_BUFFER_ANDROID.
 */

#include "server_internal.h"
#include "android-wlegl-server-protocol.h"
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG "TrierarchWlegl"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* Not part of stable NDK headers, but present on Android devices.
 * Resolve dynamically from libandroid.so so we can build with the NDK. */
#ifndef AHARDWAREBUFFER_CREATE_FROM_HANDLE_METHOD_REGISTER
#define AHARDWAREBUFFER_CREATE_FROM_HANDLE_METHOD_REGISTER 0
#endif

/* Forward-declare so we can reference it in function typedefs before the
 * fallback definition below (when <android/native_handle.h> is missing). */
typedef struct native_handle native_handle_t;

typedef int (*PFN_AHardwareBuffer_createFromHandle)(
        const AHardwareBuffer_Desc *desc,
        const native_handle_t *handle,
        int32_t method,
        AHardwareBuffer **outBuffer);

static PFN_AHardwareBuffer_createFromHandle g_createFromHandle;
static int g_createFromHandle_inited;

static PFN_AHardwareBuffer_createFromHandle get_create_from_handle(void) {
    if (g_createFromHandle_inited) return g_createFromHandle;
    g_createFromHandle_inited = 1;
    void *lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        LOGE("dlopen(libandroid.so) failed: %s", dlerror());
        return NULL;
    }
    g_createFromHandle = (PFN_AHardwareBuffer_createFromHandle)dlsym(lib, "AHardwareBuffer_createFromHandle");
    if (!g_createFromHandle) {
        LOGE("dlsym(AHardwareBuffer_createFromHandle) failed: %s", dlerror());
    }
    return g_createFromHandle;
}

/* NDK compatibility: some NDKs don't ship <android/native_handle.h>.
 * Define a minimal native_handle_t ABI here (matches AOSP libcutils/native_handle.h). */
#ifndef NATIVE_HANDLE_H
typedef struct native_handle {
    int version; /* sizeof(native_handle_t) */
    int numFds;
    int numInts;
    int data[0];
} native_handle_t;

static inline native_handle_t *native_handle_create(int numFds, int numInts) {
    if (numFds < 0 || numInts < 0) return NULL;
    size_t size = sizeof(native_handle_t) + (size_t)(numFds + numInts) * sizeof(int);
    native_handle_t *h = (native_handle_t *)calloc(1, size);
    if (!h) return NULL;
    h->version = (int)sizeof(native_handle_t);
    h->numFds = numFds;
    h->numInts = numInts;
    return h;
}

static inline int native_handle_delete(native_handle_t *h) {
    free(h);
    return 0;
}
#define NATIVE_HANDLE_H 1
#endif

/* Set by compositor.c so buffer destroy can clear surface refs */
extern struct wayland_server *g_wayland_server;

struct wlegl_handle_state {
    struct wl_resource *resource;
    int32_t expected_fds;
    int32_t got_fds;
    int *fds; /* owned */
    int32_t num_ints;
    int32_t *ints; /* owned */
};

static void wlegl_handle_destroy_impl(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static void wlegl_handle_add_fd(struct wl_client *client, struct wl_resource *resource, int32_t fd) {
    (void)client;
    struct wlegl_handle_state *st = wl_resource_get_user_data(resource);
    if (!st) {
        if (fd >= 0) close(fd);
        return;
    }
    if (st->got_fds >= st->expected_fds) {
        if (fd >= 0) close(fd);
        wl_resource_post_error(resource, ANDROID_WLEGL_HANDLE_ERROR_TOO_MANY_FDS, "too many fds");
        return;
    }
    st->fds[st->got_fds++] = fd;
}

static const struct android_wlegl_handle_interface wlegl_handle_impl = {
    .add_fd = wlegl_handle_add_fd,
    .destroy = wlegl_handle_destroy_impl,
};

static void wlegl_handle_resource_destroy(struct wl_resource *resource) {
    struct wlegl_handle_state *st = wl_resource_get_user_data(resource);
    wl_resource_set_user_data(resource, NULL);
    if (!st) return;
    for (int i = 0; i < st->got_fds; i++) {
        if (st->fds && st->fds[i] >= 0) close(st->fds[i]);
    }
    free(st->fds);
    free(st->ints);
    free(st);
}

static struct wlegl_handle_state *wlegl_handle_from_resource(struct wl_resource *res) {
    if (!res) return NULL;
    return (struct wlegl_handle_state *)wl_resource_get_user_data(res);
}

static void ahb_wl_buffer_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct wl_buffer_interface ahb_wl_buffer_iface = { .destroy = ahb_wl_buffer_destroy };

static void ahb_wl_buffer_resource_destroy(struct wl_resource *resource) {
    struct ahb_buffer *ab = wl_resource_get_user_data(resource);
    wl_resource_set_user_data(resource, NULL);
    if (!ab) return;

    if (g_wayland_server) {
        pthread_mutex_lock(&g_wayland_server->surfaces_mutex);
        struct compositor_surface *surf;
        wl_list_for_each(surf, &g_wayland_server->surfaces, link) {
            if (surf->current_buffer && surf->current_buffer->type == BUF_AHB
                    && surf->current_buffer->u.ahb == ab)
                surf->current_buffer->u.ahb = NULL;
            if (surf->pending_buffer && surf->pending_buffer->type == BUF_AHB
                    && surf->pending_buffer->u.ahb == ab)
                surf->pending_buffer->u.ahb = NULL;
        }
        pthread_mutex_unlock(&g_wayland_server->surfaces_mutex);
    }

    if (ab->ahb) {
        AHardwareBuffer_release((AHardwareBuffer *)ab->ahb);
        ab->ahb = NULL;
    }
    ab->resource = NULL;
    free(ab);
}

struct ahb_buffer *ahb_buffer_try_from_wl_resource(struct wl_resource *buf_res) {
    if (!buf_res) return NULL;
    void *u = wl_resource_get_user_data(buf_res);
    if (!u) return NULL;
    struct ahb_buffer *ab = (struct ahb_buffer *)u;
    if (ab->magic != TRIERARCH_AHB_MAGIC) return NULL;
    return ab;
}

static int fourcc_to_ahb_format(uint32_t fourcc) {
    /* Best-effort mapping: focus on the formats we actually care about. */
    switch (fourcc) {
        case 0x34325258u: /* XRGB8888 */
        case 0x34325241u: /* ARGB8888 */
            return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
        case 0x34324142u: /* BGRA8888 */
        case 0x34324241u: /* ABGR8888 */
            /* Some NDK headers don't define this even if the runtime supports it. */
#ifdef AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM
            return AHARDWAREBUFFER_FORMAT_B8G8R8A8_UNORM;
#else
            return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
#endif
        default:
            return 0;
    }
}

static void wlegl_create_handle(struct wl_client *client, struct wl_resource *resource,
        uint32_t id, int32_t num_fds, struct wl_array *ints) {
    (void)resource;
    if (num_fds < 0) {
        wl_client_post_no_memory(client);
        return;
    }
    struct wlegl_handle_state *st = calloc(1, sizeof(*st));
    if (!st) {
        wl_client_post_no_memory(client);
        return;
    }
    st->expected_fds = num_fds;
    st->got_fds = 0;
    st->fds = calloc((size_t)num_fds, sizeof(int));
    if (num_fds > 0 && !st->fds) {
        free(st);
        wl_client_post_no_memory(client);
        return;
    }
    for (int i = 0; i < num_fds; i++) st->fds[i] = -1;

    st->num_ints = (int32_t)((ints && ints->size) ? (ints->size / sizeof(int32_t)) : 0);
    if (st->num_ints > 0) {
        st->ints = calloc((size_t)st->num_ints, sizeof(int32_t));
        if (!st->ints) {
            free(st->fds);
            free(st);
            wl_client_post_no_memory(client);
            return;
        }
        memcpy(st->ints, ints->data, (size_t)st->num_ints * sizeof(int32_t));
    }

    struct wl_resource *hres = wl_resource_create(client, &android_wlegl_handle_interface, 1, id);
    if (!hres) {
        free(st->fds);
        free(st->ints);
        free(st);
        wl_client_post_no_memory(client);
        return;
    }
    st->resource = hres;
    wl_resource_set_implementation(hres, &wlegl_handle_impl, st, wlegl_handle_resource_destroy);
}

static void wlegl_create_buffer(struct wl_client *client, struct wl_resource *resource,
        uint32_t id, int32_t width, int32_t height, int32_t stride, int32_t format,
        int32_t usage, struct wl_resource *native_handle_res) {
    (void)resource;
    struct wlegl_handle_state *st = wlegl_handle_from_resource(native_handle_res);
    if (!st) {
        wl_resource_post_error(resource, ANDROID_WLEGL_ERROR_BAD_HANDLE, "bad handle");
        return;
    }
    if (st->got_fds != st->expected_fds) {
        wl_resource_post_error(resource, ANDROID_WLEGL_ERROR_BAD_HANDLE, "incomplete handle");
        return;
    }
    if (width <= 0 || height <= 0 || stride <= 0) {
        wl_resource_post_error(resource, ANDROID_WLEGL_ERROR_BAD_VALUE, "bad size/stride");
        return;
    }

    int ahb_format = fourcc_to_ahb_format((uint32_t)format);
    if (ahb_format == 0) {
        LOGE("android_wlegl create_buffer: unsupported format=0x%x", (unsigned)format);
        wl_resource_post_error(resource, ANDROID_WLEGL_ERROR_BAD_VALUE, "unsupported format");
        return;
    }
    if (st->expected_fds <= 0) {
        LOGE("android_wlegl create_buffer: expected_fds=%d (need >=1)", st->expected_fds);
        wl_resource_post_error(resource, ANDROID_WLEGL_ERROR_BAD_HANDLE, "no fds");
        return;
    }

    /* Build a native_handle from fds + int payload, then wrap as AHardwareBuffer. */
    int num_ints = st->num_ints;
    native_handle_t *nh = native_handle_create(st->expected_fds, num_ints);
    if (!nh) {
        wl_client_post_no_memory(client);
        return;
    }
    for (int i = 0; i < st->expected_fds; i++)
        nh->data[i] = st->fds[i];
    for (int i = 0; i < num_ints; i++)
        nh->data[st->expected_fds + i] = st->ints[i];

    AHardwareBuffer_Desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.width = (uint32_t)width;
    desc.height = (uint32_t)height;
    desc.layers = 1;
    desc.format = (uint32_t)ahb_format;
    desc.usage = (uint64_t)(uint32_t)usage;
    desc.stride = (uint32_t)stride;

    AHardwareBuffer *ahb = NULL;
    PFN_AHardwareBuffer_createFromHandle pCreate = get_create_from_handle();
    if (!pCreate) {
        /* We handed fds into nh; do not close them here. Destroy only the handle container. */
        native_handle_delete(nh);
        wl_resource_post_error(resource, ANDROID_WLEGL_ERROR_BAD_HANDLE, "AHardwareBuffer_createFromHandle missing");
        return;
    }
    int rc = pCreate(&desc, (const native_handle_t *)nh,
            AHARDWAREBUFFER_CREATE_FROM_HANDLE_METHOD_REGISTER, &ahb);

    /* We handed fds into nh; do not close them here. Destroy only the handle container. */
    native_handle_delete(nh);

    if (rc != 0 || !ahb) {
        LOGE("AHardwareBuffer_createFromHandle failed rc=%d errno=%d w=%d h=%d stride=%d fmt=0x%x usage=0x%x fds=%d ints=%d",
                rc, errno, width, height, stride, (unsigned)format, (unsigned)usage, st->expected_fds, st->num_ints);
        wl_resource_post_error(resource, ANDROID_WLEGL_ERROR_BAD_HANDLE, "ahb create failed");
        return;
    }

    struct ahb_buffer *ab = calloc(1, sizeof(*ab));
    if (!ab) {
        AHardwareBuffer_release(ahb);
        wl_client_post_no_memory(client);
        return;
    }
    ab->magic = TRIERARCH_AHB_MAGIC;
    ab->resource = NULL;
    ab->ahb = ahb;
    ab->width = width;
    ab->height = height;
    ab->stride = stride;
    ab->format = (uint32_t)ahb_format;
    ab->usage = (uint64_t)(uint32_t)usage;
    ab->owner = NULL;

    struct wl_resource *buf_res = wl_resource_create(client, &wl_buffer_interface, 1, id);
    if (!buf_res) {
        AHardwareBuffer_release(ahb);
        free(ab);
        wl_client_post_no_memory(client);
        return;
    }
    ab->resource = buf_res;
    wl_resource_set_implementation(buf_res, &ahb_wl_buffer_iface, ab, ahb_wl_buffer_resource_destroy);

    static unsigned created_logged;
    if (created_logged < 64) {
        LOGI("android_wlegl wl_buffer created: %dx%d stride=%d fmt=0x%x usage=0x%x fds=%d ints=%d",
                width, height, stride, (unsigned)format, (unsigned)usage,
                st->expected_fds, st->num_ints);
        created_logged++;
    }
}

static void wlegl_destroy(struct wl_client *client, struct wl_resource *resource) {
    (void)client;
    wl_resource_destroy(resource);
}

static const struct android_wlegl_interface wlegl_impl = {
    .create_handle = wlegl_create_handle,
    .create_buffer = wlegl_create_buffer,
};

void android_wlegl_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
    (void)data;
    if (version > 1) version = 1;
    struct wl_resource *res = wl_resource_create(client, &android_wlegl_interface, version, id);
    if (!res) {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(res, &wlegl_impl, NULL, NULL);
    {
        static unsigned bind_logged;
        if (bind_logged < 128) {
            pid_t pid = (pid_t)-1;
            uid_t uid = 0;
            gid_t gid = 0;
            wl_client_get_credentials(client, &pid, &uid, &gid);
            LOGI("bind android_wlegl v%u pid=%d uid=%u (n=%u)", version, (int)pid, (unsigned)uid, bind_logged + 1);
            bind_logged++;
        }
    }

    /* Advertise common formats (fourcc). Keep the list small and practical. */
    android_wlegl_send_format(res, 0x34325258u); /* XRGB8888 */
    android_wlegl_send_format(res, 0x34325241u); /* ARGB8888 */
    android_wlegl_send_format(res, 0x34324142u); /* BGRA8888 */
}

