/*
 * JNI: lifecycle, render thread, dispatch thread, pointer input, display.
 */
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "compositor.h"
#include "server_internal.h"
#include "renderer.h"

#define LOG_TAG "WaylandJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern volatile const char *g_wayland_checkpoint;

static void crash_handler(int sig, siginfo_t *info, void *uctx) {
    (void)uctx;
    __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, "CRASH signal %d addr=%p checkpoint=%s",
        sig, info->si_addr, g_wayland_checkpoint ? g_wayland_checkpoint : "?");
    struct sigaction sa = { .sa_handler = SIG_DFL };
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
    raise(sig);
}

static wayland_server_t *g_server;
static renderer_context_t *g_renderer;
static ANativeWindow *g_window;
static pthread_t g_render_thread, g_dispatch_thread;
static int g_render_created, g_dispatch_created;
static volatile int g_running, g_stop_dispatch;

/* ---- Key event queue (JNI thread -> Wayland thread) ----
 *
 * libwayland-server is not thread-safe. We must NOT call wl_keyboard_send_* from
 * arbitrary Java threads. Instead, enqueue key events from JNI and drain them
 * from the Wayland dispatch/render thread.
 */
struct queued_key_event {
    uint32_t time_ms;
    uint32_t key_linux;
    uint32_t state;
};

static pthread_mutex_t g_keyq_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct queued_key_event *g_keyq = NULL;
static size_t g_keyq_cap = 0;
static size_t g_keyq_head = 0;
static size_t g_keyq_tail = 0;
static size_t g_keyq_len = 0;
static int g_keyq_drop_logged = 0;

static void keyq_init_if_needed(void) {
    if (g_keyq) return;
    g_keyq_cap = 8192; /* enough for normal typing; paste will grow quickly */
    g_keyq = (struct queued_key_event *)calloc(g_keyq_cap, sizeof(*g_keyq));
    g_keyq_head = g_keyq_tail = g_keyq_len = 0;
}

static void keyq_push(uint32_t time_ms, uint32_t key_linux, uint32_t state) {
    pthread_mutex_lock(&g_keyq_mutex);
    keyq_init_if_needed();
    if (!g_keyq) {
        pthread_mutex_unlock(&g_keyq_mutex);
        return;
    }

    /* Hard cap to prevent unbounded memory growth. */
    const size_t HARD_CAP = 262144;
    if (g_keyq_len >= HARD_CAP) {
        if (!g_keyq_drop_logged) {
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                "keyq full (len=%zu), dropping events", g_keyq_len);
            g_keyq_drop_logged = 1;
        }
        pthread_mutex_unlock(&g_keyq_mutex);
        return;
    }

    /* Grow ring buffer if full. */
    if (g_keyq_len == g_keyq_cap) {
        size_t new_cap = g_keyq_cap * 2;
        if (new_cap > HARD_CAP) new_cap = HARD_CAP;
        struct queued_key_event *nq = (struct queued_key_event *)calloc(new_cap, sizeof(*nq));
        if (nq) {
            /* Copy in order. */
            for (size_t i = 0; i < g_keyq_len; i++) {
                size_t idx = (g_keyq_head + i) % g_keyq_cap;
                nq[i] = g_keyq[idx];
            }
            free(g_keyq);
            g_keyq = nq;
            g_keyq_cap = new_cap;
            g_keyq_head = 0;
            g_keyq_tail = g_keyq_len;
        }
    }

    if (g_keyq_len < g_keyq_cap) {
        g_keyq[g_keyq_tail].time_ms = time_ms;
        g_keyq[g_keyq_tail].key_linux = key_linux;
        g_keyq[g_keyq_tail].state = state;
        g_keyq_tail = (g_keyq_tail + 1) % g_keyq_cap;
        g_keyq_len++;
    }
    pthread_mutex_unlock(&g_keyq_mutex);
}

static size_t keyq_drain(struct queued_key_event *out, size_t max) {
    size_t n = 0;
    pthread_mutex_lock(&g_keyq_mutex);
    while (n < max && g_keyq_len > 0 && g_keyq) {
        out[n++] = g_keyq[g_keyq_head];
        g_keyq_head = (g_keyq_head + 1) % g_keyq_cap;
        g_keyq_len--;
    }
    if (g_keyq_len == 0) {
        g_keyq_drop_logged = 0;
    }
    pthread_mutex_unlock(&g_keyq_mutex);
    return n;
}

static void drain_key_events_on_wayland_thread(void) {
    if (!g_server) return;
    /*
     * IMPORTANT: Do not drain-to-empty in one tick, otherwise long pastes will
     * starve render/dispatch and appear as a black screen. Process a bounded
     * number of events per call, then return so the frame loop can render.
     */
    const size_t MAX_EVENTS_PER_TICK = 512;
    const long MAX_NS_PER_TICK = 2L * 1000L * 1000L; /* ~2ms budget */
    size_t processed = 0;
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    struct queued_key_event batch[512];
    while (processed < MAX_EVENTS_PER_TICK) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ns = (now.tv_sec - t0.tv_sec) * 1000000000L + (now.tv_nsec - t0.tv_nsec);
        if (elapsed_ns >= MAX_NS_PER_TICK) break;

        size_t want = sizeof(batch) / sizeof(batch[0]);
        if (want > (MAX_EVENTS_PER_TICK - processed)) want = (MAX_EVENTS_PER_TICK - processed);
        size_t n = keyq_drain(batch, want);
        if (n == 0) break;
        processed += n;
        for (size_t i = 0; i < n; i++) {
            compositor_keyboard_key_event(g_server, batch[i].time_ms, batch[i].key_linux, batch[i].state);
        }
    }
}

static void *dispatch_loop(void *arg) {
    (void)arg;
    struct timespec last;
    clock_gettime(CLOCK_MONOTONIC, &last);
    while (!g_stop_dispatch && g_server) {
        drain_key_events_on_wayland_thread();
        compositor_dispatch_timeout(g_server, 16);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long ms = (now.tv_sec - last.tv_sec) * 1000 + (now.tv_nsec - last.tv_nsec) / 1000000;
        if (ms >= 16) {
            compositor_send_frame_callbacks(g_server);
            compositor_send_ping_to_clients(g_server);
            last = now;
        }
    }
    return NULL;
}

static void *render_loop(void *arg) {
    (void)arg;
    while (g_running && g_renderer && renderer_is_valid(g_renderer)) {
        g_wayland_checkpoint = "dispatch";
        if (g_server) {
            drain_key_events_on_wayland_thread();
            compositor_dispatch(g_server);
        }
        g_wayland_checkpoint = "render";
        if (!renderer_render(g_renderer, (struct wayland_server *)g_server)) break;
        if (g_server) {
            compositor_send_frame_callbacks(g_server);
            compositor_send_ping_to_clients(g_server);
        }
    }
    if (g_renderer && renderer_is_valid(g_renderer)) renderer_release_context(g_renderer);
    return NULL;
}

static void stop_dispatch(void) {
    if (g_dispatch_created) { g_stop_dispatch = 1; pthread_join(g_dispatch_thread, NULL); g_dispatch_created = 0; g_stop_dispatch = 0; }
}
static void start_dispatch(void) {
    if (g_dispatch_created || !g_server) return;
    g_stop_dispatch = 0;
    if (pthread_create(&g_dispatch_thread, NULL, dispatch_loop, NULL) == 0) g_dispatch_created = 1;
}

JNIEXPORT void JNICALL Java_app_trierarch_WaylandBridge_nativeStartServer(JNIEnv *env, jobject thiz, jstring runtime_dir) {
    (void)thiz;
    if (!runtime_dir) return;
    const char *dir = (*env)->GetStringUTFChars(env, runtime_dir, NULL);
    if (!dir) return;
    mkdir(dir, 0700);
    if (!g_server) g_server = compositor_create(dir);
    (*env)->ReleaseStringUTFChars(env, runtime_dir, dir);
    if (g_server && !g_render_created) start_dispatch();
}

JNIEXPORT void JNICALL Java_app_trierarch_WaylandBridge_nativeSurfaceCreated(JNIEnv *env, jobject thiz, jobject surface, jstring runtime_dir, jint resolution_percent, jint scale_percent) {
    (void)thiz;
    if (!surface || !runtime_dir) return;
    const char *dir = (*env)->GetStringUTFChars(env, runtime_dir, NULL);
    if (!dir) return;
    mkdir(dir, 0700);
    struct sigaction sa; sa.sa_sigaction = crash_handler; sigemptyset(&sa.sa_mask); sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL); sigaction(SIGABRT, &sa, NULL);
    if (!g_server) g_server = compositor_create(dir);
    (*env)->ReleaseStringUTFChars(env, runtime_dir, dir);
    if (!g_server) return;
    stop_dispatch();
    g_running = 0;
    if (g_render_created) { pthread_join(g_render_thread, NULL); g_render_created = 0; }
    if (g_renderer) { renderer_destroy(g_renderer); g_renderer = NULL; }
    if (g_window) { ANativeWindow_release(g_window); g_window = NULL; }
    usleep(50000);
    g_window = ANativeWindow_fromSurface(env, surface);
    if (!g_window) { start_dispatch(); return; }
    g_renderer = renderer_create(g_window, (struct wayland_server *)g_server);
    if (!g_renderer) { ANativeWindow_release(g_window); g_window = NULL; start_dispatch(); return; }
    int pw = 0, ph = 0;
    renderer_get_size(g_renderer, &pw, &ph);
    int rp = (resolution_percent >= 10 && resolution_percent <= 100) ? (int)resolution_percent : 100;
    int sp = (scale_percent >= 10 && scale_percent <= 100) ? (int)scale_percent : 100;
    /* Logical size = physical * resolution% * scale%; client draws for that, we render to full screen */
    int32_t lw = (pw > 0 && rp > 0 && sp > 0) ? (pw * rp * sp + 5000) / 10000 : pw;
    int32_t lh = (ph > 0 && rp > 0 && sp > 0) ? (ph * rp * sp + 5000) / 10000 : ph;
    if (lw < 1) lw = 1;
    if (lh < 1) lh = 1;
    compositor_set_output_override(g_server, lw, lh);
    if (pw > 0 && ph > 0) compositor_set_output_size(g_server, lw, lh, pw, ph);
    g_running = 1;
    g_render_created = (pthread_create(&g_render_thread, NULL, render_loop, NULL) == 0);
}

JNIEXPORT void JNICALL Java_app_trierarch_WaylandBridge_nativeSurfaceDestroyed(JNIEnv *env, jobject thiz) {
    (void)env;(void)thiz;
    g_running = 0;
    if (g_render_created) { pthread_join(g_render_thread, NULL); g_render_created = 0; }
    if (g_renderer) { renderer_destroy(g_renderer); g_renderer = NULL; }
    if (g_window) { ANativeWindow_release(g_window); g_window = NULL; }
    start_dispatch();
}

JNIEXPORT void JNICALL Java_app_trierarch_WaylandBridge_nativeOutputSizeChanged(JNIEnv *env, jobject thiz, jint width, jint height, jint resolution_percent, jint scale_percent) {
    (void)env;(void)thiz;
    if (!g_server || width <= 0 || height <= 0) return;
    int rp = (resolution_percent >= 10 && resolution_percent <= 100) ? (int)resolution_percent : 100;
    int sp = (scale_percent >= 10 && scale_percent <= 100) ? (int)scale_percent : 100;
    int32_t lw = (width * rp * sp + 5000) / 10000;
    int32_t lh = (height * rp * sp + 5000) / 10000;
    if (lw < 1) lw = 1;
    if (lh < 1) lh = 1;
    compositor_set_output_override(g_server, lw, lh);
    compositor_set_output_size(g_server, lw, lh, (int32_t)width, (int32_t)height);
}

JNIEXPORT void JNICALL Java_app_trierarch_WaylandBridge_nativeOnPointerEvent(JNIEnv *env, jobject thiz, jfloat x, jfloat y, jint action, jint timeMs) {
    (void)env;(void)thiz;
    if (!g_server) return;
    compositor_pointer_event(g_server, (float)x, (float)y, (int)action, (uint32_t)timeMs);
}

JNIEXPORT void JNICALL Java_app_trierarch_WaylandBridge_nativeOnPointerAxis(JNIEnv *env, jobject thiz, jfloat deltaX, jfloat deltaY, jint timeMs, jint axisSource) {
    (void)env;(void)thiz;
    if (!g_server) return;
    compositor_pointer_axis_event(g_server, (uint32_t)timeMs, (float)deltaX, (float)deltaY, (uint32_t)axisSource);
}

JNIEXPORT void JNICALL Java_app_trierarch_WaylandBridge_nativeOnPointerRightClick(JNIEnv *env, jobject thiz, jfloat x, jfloat y, jint timeMs) {
    (void)env;(void)thiz;
    if (!g_server) return;
    compositor_pointer_right_click(g_server, (uint32_t)timeMs, (float)x, (float)y);
}

JNIEXPORT void JNICALL Java_app_trierarch_WaylandBridge_nativeSetCursorPhysical(JNIEnv *env, jobject thiz, jfloat x, jfloat y) {
    (void)env;(void)thiz;
    if (!g_server) return;
    compositor_set_cursor_physical(g_server, (float)x, (float)y);
}

JNIEXPORT void JNICALL Java_app_trierarch_WaylandBridge_nativeSetCursorVisible(JNIEnv *env, jobject thiz, jboolean visible) {
    (void)env;(void)thiz;
    if (!g_server) return;
    compositor_set_cursor_visible(g_server, visible ? true : false);
}

JNIEXPORT void JNICALL Java_app_trierarch_WaylandBridge_nativeStopWayland(JNIEnv *env, jobject thiz) {
    (void)env;(void)thiz;
    g_running = 0;
    if (g_render_created) { pthread_join(g_render_thread, NULL); g_render_created = 0; }
    stop_dispatch();
    wayland_server_t *s = g_server; g_server = NULL;
    if (s) compositor_destroy(s);
}

JNIEXPORT jboolean JNICALL Java_app_trierarch_WaylandBridge_nativeIsWaylandReady(JNIEnv *env, jobject thiz) {
    (void)env;(void)thiz;
    return (g_renderer && renderer_is_valid(g_renderer)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jintArray JNICALL Java_app_trierarch_WaylandBridge_nativeGetOutputSize(JNIEnv *env, jobject thiz) {
    (void)thiz;
    jintArray out = (*env)->NewIntArray(env, 2);
    if (!out || !g_server) return out;
    int32_t w = 0, h = 0;
    compositor_get_output_size(g_server, &w, &h);
    jint arr[] = { (jint)w, (jint)h };
    (*env)->SetIntArrayRegion(env, out, 0, 2, arr);
    return out;
}

JNIEXPORT jstring JNICALL Java_app_trierarch_WaylandBridge_nativeGetSocketDir(JNIEnv *env, jobject thiz, jstring files_dir) {
    (void)thiz;
    if (!files_dir) return NULL;
    const char *path = (*env)->GetStringUTFChars(env, files_dir, NULL);
    if (!path) return NULL;
    static char buf[512];
    snprintf(buf, sizeof(buf), "%s/usr/tmp", path);
    (*env)->ReleaseStringUTFChars(env, files_dir, path);
    return (*env)->NewStringUTF(env, buf);
}

JNIEXPORT jboolean JNICALL Java_app_trierarch_WaylandBridge_nativeHasActiveClients(JNIEnv *env, jobject thiz) {
    (void)env;(void)thiz;
    if (!g_server) return JNI_FALSE;
    return compositor_has_toplevel_client(g_server) ? JNI_TRUE : JNI_FALSE;
}

/*
 * Android keycode (KeyEvent.KEYCODE_*) to Linux evdev key code (KEY_* from input-event-codes.h).
 * Letters: Android uses 29-54 (A-Z order); evdev uses physical QWERTY positions.
 * Unmapped => 0 (ignore).
 */
static uint32_t android_keycode_to_linux(int android_keycode) {
    switch (android_keycode) {
        case 4:   return 1;   /* KEYCODE_BACK -> KEY_ESC */
        case 7:   return 11;  /* KEYCODE_0 -> KEY_0 */
        case 8:   return 2;   case 9:   return 3;   case 10:  return 4;   case 11:  return 5;
        case 12:  return 6;   case 13:  return 7;   case 14:  return 8;   case 15:  return 9;
        case 16:  return 10;  /* KEYCODE_1..9 -> KEY_1..KEY_9 */
        case 19:  return 103; /* KEYCODE_DPAD_UP -> KEY_UP */
        case 20:  return 108; /* KEYCODE_DPAD_DOWN -> KEY_DOWN */
        case 21:  return 105; /* KEYCODE_DPAD_LEFT -> KEY_LEFT */
        case 22:  return 106; /* KEYCODE_DPAD_RIGHT -> KEY_RIGHT */
        case 28:  return 28;  /* KEYCODE_ENTER (num) -> KEY_ENTER */
        /* Android A(29)..Z(54) -> evdev KEY_* by physical key (QWERTY) */
        case 29:  return 30;  /* A -> KEY_A */
        case 30:  return 48;  /* B -> KEY_B */
        case 31:  return 46;  /* C -> KEY_C */
        case 32:  return 32;  /* D -> KEY_D */
        case 33:  return 18;  /* E -> KEY_E */
        case 34:  return 33;  /* F -> KEY_F */
        case 35:  return 34;  /* G -> KEY_G */
        case 36:  return 35;  /* H -> KEY_H */
        case 37:  return 23;  /* I -> KEY_I */
        case 38:  return 36;  /* J -> KEY_J */
        case 39:  return 37;  /* K -> KEY_K */
        case 40:  return 38;  /* L -> KEY_L */
        case 41:  return 50;  /* M -> KEY_M */
        case 42:  return 49;  /* N -> KEY_N */
        case 43:  return 24;  /* O -> KEY_O */
        case 44:  return 25;  /* P -> KEY_P */
        case 45:  return 16;  /* Q -> KEY_Q */
        case 46:  return 19;  /* R -> KEY_R */
        case 47:  return 31;  /* S -> KEY_S */
        case 48:  return 20;  /* T -> KEY_T */
        case 49:  return 22;  /* U -> KEY_U */
        case 50:  return 47;  /* V -> KEY_V */
        case 51:  return 17;  /* W -> KEY_W */
        case 52:  return 45;  /* X -> KEY_X */
        case 53:  return 21;  /* Y -> KEY_Y */
        case 54:  return 44;  /* Z -> KEY_Z */
        case 55:  return 51;  /* KEYCODE_COMMA -> KEY_COMMA */
        case 56:  return 52;  /* KEYCODE_PERIOD -> KEY_DOT */
        case 57:  return 56;  /* KEYCODE_ALT_LEFT -> KEY_LEFTALT */
        case 58:  return 100; /* KEYCODE_ALT_RIGHT -> KEY_RIGHTALT */
        case 59:  return 42;  /* KEYCODE_SHIFT_LEFT -> KEY_LEFTSHIFT */
        case 60:  return 54;  /* KEYCODE_SHIFT_RIGHT -> KEY_RIGHTSHIFT */
        case 61:  return 15;  /* KEYCODE_TAB -> KEY_TAB */
        case 62:  return 57;  /* KEYCODE_SPACE -> KEY_SPACE */
        case 66:  return 28;  /* KEYCODE_ENTER -> KEY_ENTER */
        case 67:  return 14;  /* KEYCODE_DEL -> KEY_BACKSPACE */
        case 68:  return 41;  /* KEYCODE_GRAVE -> KEY_GRAVE */
        case 69:  return 12;  /* KEYCODE_MINUS -> KEY_MINUS */
        case 70:  return 13;  /* KEYCODE_EQUALS -> KEY_EQUAL */
        case 71:  return 26;  /* KEYCODE_LEFT_BRACKET -> KEY_LEFTBRACE */
        case 72:  return 27;  /* KEYCODE_RIGHT_BRACKET -> KEY_RIGHTBRACE */
        case 73:  return 43;  /* KEYCODE_BACKSLASH -> KEY_BACKSLASH */
        case 74:  return 39;  /* KEYCODE_SEMICOLON -> KEY_SEMICOLON */
        case 75:  return 40;  /* KEYCODE_APOSTROPHE -> KEY_APOSTROPHE */
        case 76:  return 53;  /* KEYCODE_SLASH -> KEY_SLASH */
        case 111: return 1;   /* KEYCODE_ESCAPE -> KEY_ESC */
        case 112: return 111; /* KEYCODE_FORWARD_DEL -> KEY_DELETE */
        case 113: return 29;  /* KEYCODE_CTRL_LEFT -> KEY_LEFTCTRL */
        case 114: return 97;  /* KEYCODE_CTRL_RIGHT -> KEY_RIGHTCTRL */
        case 117: return 125; /* KEYCODE_META_LEFT -> KEY_LEFTMETA */
        case 118: return 126; /* KEYCODE_META_RIGHT -> KEY_RIGHTMETA */
        case 122: return 102; /* KEYCODE_MOVE_HOME -> KEY_HOME */
        case 123: return 107; /* KEYCODE_MOVE_END -> KEY_END */
        case 124: return 110; /* KEYCODE_INSERT -> KEY_INSERT */
        default:  return 0;
    }
}

JNIEXPORT void JNICALL Java_app_trierarch_WaylandBridge_nativeOnKeyEvent(JNIEnv *env, jobject thiz,
        jint keyCode, jint metaState, jboolean isDown, jlong timeMs) {
    (void)env;(void)thiz;(void)metaState;
    if (!g_server) return;
    uint32_t key_linux = android_keycode_to_linux(keyCode);
    if (key_linux == 0) return;
    uint32_t time = (uint32_t)(timeMs & 0xFFFFFFFFu);
    uint32_t state = isDown ? 1 : 0;  /* WL_KEYBOARD_KEY_STATE_PRESSED / RELEASED */
    keyq_push(time, key_linux, state);
}
