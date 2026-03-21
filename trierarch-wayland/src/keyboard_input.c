/*
 * Keyboard input: track wl_keyboard resources; keyboard focus follows pointer focus.
 * Deliver key events to focused client (e.g. KWin for approach A).
 * Send an xkb_v1 keymap so clients (Qt/GTK) can interpret modifiers/ISO-14755 consistently.
 * We copy a bundled keymap_us.xkb into srv->runtime_dir on app start.
 */
#include "server_internal.h"
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#define WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP  0
#define WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1    1
#define WL_KEYBOARD_KEY_STATE_RELEASED       0
#define WL_KEYBOARD_KEY_STATE_PRESSED        1

/* Common XKB-style modifier bitmasks. We build wl_keyboard.modifiers from these. */
#define MOD_SHIFT_MASK 0x01
#define MOD_CTRL_MASK  0x04
/* Keep bits non-overlapping with Ctrl; empirically on this system Ctrl uses 0x04. */
#define MOD_ALT_MASK   0x02
#define MOD_META_MASK  0x08

/* Linux evdev keycodes (must match jni_bridge.c mappings). */
#define KEY_LINUX_LEFTSHIFT   42
#define KEY_LINUX_RIGHTSHIFT  54
#define KEY_LINUX_LEFTCTRL     29
#define KEY_LINUX_RIGHTCTRL    97
#define KEY_LINUX_LEFTALT      56
#define KEY_LINUX_RIGHTALT     100
#define KEY_LINUX_LEFTMETA     125
#define KEY_LINUX_RIGHTMETA    126

static void send_keymap_to_resource(struct wl_resource *keyboard_res, struct wayland_server *srv) {
    if (!keyboard_res || !srv || !srv->runtime_dir) return;

    char path[512];
    snprintf(path, sizeof(path), "%s/keymap_us.xkb", srv->runtime_dir);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        /* Fallback: keep old behavior if keymap missing. */
        int nfd = open("/dev/null", O_RDONLY);
        if (nfd < 0) return;
        wl_keyboard_send_keymap(keyboard_res, WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP, nfd, 0);
        close(nfd);
        wl_keyboard_send_repeat_info(keyboard_res, 25, 400);
        return;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0) {
        close(fd);
        int nfd = open("/dev/null", O_RDONLY);
        if (nfd < 0) return;
        wl_keyboard_send_keymap(keyboard_res, WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP, nfd, 0);
        close(nfd);
        wl_keyboard_send_repeat_info(keyboard_res, 25, 400);
        return;
    }

    wl_keyboard_send_keymap(keyboard_res, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, (uint32_t)st.st_size);
    close(fd);
    wl_keyboard_send_repeat_info(keyboard_res, 25, 400);
}

static void keyboard_send_enter(struct wayland_server *srv,
        struct compositor_surface *surf, struct wl_resource *keyboard_res) {
    if (!surf || !surf->resource || !keyboard_res)
        return;
    uint32_t serial = wl_display_next_serial(srv->display);
    struct wl_array keys;
    wl_array_init(&keys);
    wl_keyboard_send_enter(keyboard_res, serial, surf->resource, &keys);
    wl_array_release(&keys);
    if (wl_resource_get_version(keyboard_res) >= 4)
        wl_keyboard_send_modifiers(keyboard_res, wl_display_next_serial(srv->display),
            srv->keyboard_mods_depressed, 0, 0, 0);
}

static void keyboard_send_leave(struct wayland_server *srv,
        struct compositor_surface *surf, struct wl_resource *keyboard_res) {
    if (!surf || !surf->resource || !keyboard_res)
        return;
    uint32_t serial = wl_display_next_serial(srv->display);
    wl_keyboard_send_leave(keyboard_res, serial, surf->resource);
}

void keyboard_focus_update(struct wayland_server *srv, struct compositor_surface *new_focus) {
    if (!srv)
        return;
    struct compositor_surface *old = srv->keyboard_focus;
    if (old == new_focus)
        return;
    srv->keyboard_focus = new_focus;

    struct wl_client *old_client = old && old->resource ? wl_resource_get_client(old->resource) : NULL;
    struct wl_client *new_client = new_focus && new_focus->resource ? wl_resource_get_client(new_focus->resource) : NULL;

    struct input_resource_node *node;
    wl_list_for_each(node, &srv->keyboard_resources, link) {
        struct wl_client *res_client = wl_resource_get_client(node->resource);
        if (old_client && res_client == old_client)
            keyboard_send_leave(srv, old, node->resource);
        if (new_client && res_client == new_client) {
            send_keymap_to_resource(node->resource, srv);
            keyboard_send_enter(srv, new_focus, node->resource);
        }
    }
}

void compositor_keyboard_key_event(wayland_server_t *srv_opaque, uint32_t time_ms,
        uint32_t key_linux, uint32_t state) {
    if (!srv_opaque)
        return;
    struct wayland_server *srv = (struct wayland_server *)srv_opaque;
    struct compositor_surface *focus = srv->keyboard_focus;
    if (!focus || !focus->resource)
        return;
    struct wl_client *focus_client = wl_resource_get_client(focus->resource);
    uint32_t serial = wl_display_next_serial(srv->display);
    struct input_resource_node *node;

    /* Update modifier state on modifier keys, then notify client. */
    bool mods_changed = false;
    if (key_linux == KEY_LINUX_LEFTSHIFT || key_linux == KEY_LINUX_RIGHTSHIFT) {
        if (state == WL_KEYBOARD_KEY_STATE_PRESSED) srv->keyboard_shift_count++;
        else if (srv->keyboard_shift_count > 0) srv->keyboard_shift_count--;
        mods_changed = true;
    } else if (key_linux == KEY_LINUX_LEFTCTRL || key_linux == KEY_LINUX_RIGHTCTRL) {
        if (state == WL_KEYBOARD_KEY_STATE_PRESSED) srv->keyboard_ctrl_count++;
        else if (srv->keyboard_ctrl_count > 0) srv->keyboard_ctrl_count--;
        mods_changed = true;
    } else if (key_linux == KEY_LINUX_LEFTALT || key_linux == KEY_LINUX_RIGHTALT) {
        if (state == WL_KEYBOARD_KEY_STATE_PRESSED) srv->keyboard_alt_count++;
        else if (srv->keyboard_alt_count > 0) srv->keyboard_alt_count--;
        mods_changed = true;
    } else if (key_linux == KEY_LINUX_LEFTMETA || key_linux == KEY_LINUX_RIGHTMETA) {
        if (state == WL_KEYBOARD_KEY_STATE_PRESSED) srv->keyboard_meta_count++;
        else if (srv->keyboard_meta_count > 0) srv->keyboard_meta_count--;
        mods_changed = true;
    }

    if (mods_changed) {
        srv->keyboard_mods_depressed = 0;
        if (srv->keyboard_shift_count) srv->keyboard_mods_depressed |= MOD_SHIFT_MASK;
        if (srv->keyboard_ctrl_count) srv->keyboard_mods_depressed |= MOD_CTRL_MASK;
        if (srv->keyboard_alt_count) srv->keyboard_mods_depressed |= MOD_ALT_MASK;
        if (srv->keyboard_meta_count) srv->keyboard_mods_depressed |= MOD_META_MASK;
    }

    wl_list_for_each(node, &srv->keyboard_resources, link) {
        if (wl_resource_get_client(node->resource) != focus_client)
            continue;

        if (mods_changed && wl_resource_get_version(node->resource) >= 4) {
            wl_keyboard_send_modifiers(node->resource, wl_display_next_serial(srv->display),
                srv->keyboard_mods_depressed, 0, 0, 0);
        }

        wl_keyboard_send_key(node->resource, serial, time_ms, key_linux, state);
    }
}
