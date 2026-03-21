# Trierarch Wayland

[中文](README.zh.md) | English

---

Minimal Wayland compositor for [Trierarch](https://github.com/Beauty114514/trierarch-app): runs inside the Android app process, exposes a single fullscreen output over SHM so that Linux GUI apps (e.g. under proot) can display on the app Surface. **Input**: pointer (touch → wl_pointer, touchpad and tablet modes) and keyboard (Android IME → wl_keyboard to focused client). Implements xdg-shell, presentation-time, viewporter, subcompositor, relative-pointer, pointer-constraints, and minimal data-device/wl_surface extensions so that full desktop environments (e.g. KDE) can run.

## Prerequisites

- Android NDK (e.g. `$ANDROID_NDK_HOME` or `$HOME/Android/Sdk/ndk/<version>`)
- **Wayland server and libffi for Android (arm64-v8a)**  
  This module links against `libwayland-server.so` and `libffi.so`. You must provide them in `libs/lib/` before building. Options:
  - **Use the provided script** (recommended): from the `trierarch-wayland` directory run  
    `./scripts/build-wayland-android.sh`  
    It builds libffi, Wayland, the compositor (ndk-build), and puts **all three** `.so` files in `out/arm64-v8a/`. Requires: `meson`, `ninja`, and host `wayland` / `wayland-protocols` (e.g. on Arch: `pacman -S meson ninja wayland wayland-protocols`). The script uses the same NDK it finds (set `ANDROID_NDK_HOME` if needed).
  - Or build [Wayland](https://gitlab.freedesktop.org/wayland/wayland) and [libffi](https://github.com/libffi/libffi) yourself for Android arm64-v8a and copy the `.so` and headers into `libs/lib/` and `libs/include/`.
  - Or use prebuilt binaries (place `libwayland-server.so` and `libffi.so` in `libs/lib/`, with Wayland server headers in `libs/include/`).

The repository already includes `libs/include/` and `libs/share/`. The script fills `libs/lib/` and can refresh `protocol/`; otherwise only the two `.so` files in `libs/lib/` are required from an external build.

## Build

**Recommended:** run the script once; it produces everything and writes to `out/arm64-v8a/`:

```bash
cd /path/to/trierarch-wayland
./scripts/build-wayland-android.sh
```

**Manual:** if you already have `libs/lib/` and `libs/include/` (wayland + libffi), build only the compositor:

```bash
cd /path/to/trierarch-wayland
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_ABI=arm64-v8a
```

Output is in `obj/local/arm64-v8a/libwayland-compositor.so`. This module is **arm64-v8a only** (see `Application.mk`).

## Use in the app

Copy **all** `.so` files from `trierarch-wayland/out/arm64-v8a/` into the app’s JNI libs:

```bash
cp /path/to/trierarch-wayland/out/arm64-v8a/*.so trierarch-app/app/src/main/jniLibs/arm64-v8a/
```

That gives you: `libwayland-compositor.so`, `libwayland-server.so`, `libffi.so`.

Load with `System.loadLibrary("wayland-compositor")` and call the JNI methods on `app.trierarch.WaylandBridge`: `nativeStartServer`, `nativeSurfaceCreated`, `nativeSurfaceDestroyed`, `nativeStopWayland`, `nativeIsWaylandReady`, `nativeGetOutputSize`, `nativeGetSocketDir`, `nativeOnPointerEvent`, `nativeSetCursorPhysical`, `nativeOnKeyEvent`, `nativeCommitTextUtf8`, `nativeHasActiveClients`.

**Multi-language input (Unicode injection):** For characters that cannot be produced by normal key mapping (e.g. CJK/emoji), Trierarch parses committed text into Unicode codepoints and injects the common Linux desktop Unicode input sequence: `Ctrl+Shift+U` + hex codepoint + `Space` (per codepoint). ASCII and control keys continue to use normal keyboard event mapping.

## Socket and runtime dir

The compositor creates a Wayland socket named **wayland-trierarch** in the runtime directory passed from the app (e.g. `getFilesDir()/usr/tmp`). Clients (e.g. under proot) must use that directory as `XDG_RUNTIME_DIR` and set `WAYLAND_DISPLAY=wayland-trierarch` to connect.

## Protocol and libs in this repo

- **protocol/** — Generated C code for the Wayland protocols used by the compositor (xdg-shell, presentation-time, viewporter, subcompositor, etc.). Other protocol files in the repo are available for future use.
- **libs/** — Headers and shared libraries for Wayland and libffi (filled by the script or by you). See Prerequisites.
- **out/arm64-v8a/** — Unified build output: all three `.so` files for the app. Created by the script; copy its contents to the app’s jniLibs.
- (Removed) `ime-bridge/` input-method bridge approach: input-method protocol exposure/permission differs across compositors, so the default path is now keyboard events + Unicode input sequence injection for broader compatibility.

License: see the root of the Trierarch repository.
