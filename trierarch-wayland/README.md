# trierarch-wayland

[中文](README.zh.md) | English

Part of the [Trierarch monorepo](https://github.com/Beauty114514/trierarch). Vendored **Wayland** / **wayland-protocols** / **libffi** trees carry their own licenses under `wayland/`, `wayland-protocols/`, `libffi/`.

## Role and what’s implemented

Minimal **Wayland compositor** running inside the Android app process: one fullscreen SHM output so Linux GUI clients (e.g. in proot) can render into the app **Surface**. **Input:** touch → `wl_pointer` (absolute / relative / touchpad modes), soft keyboard → `wl_keyboard`. **Protocols:** xdg-shell, presentation-time, viewporter, subcompositor, relative-pointer, pointer-constraints, minimal data-device / `wl_surface` pieces—enough for **KDE Plasma (Wayland)** and similar stacks.

**Unicode input:** for CJK/emoji, committed text is split into codepoints and injected using the common `Ctrl+Shift+U` + hex + `Space` sequence per codepoint; ASCII stays on normal key events.

**Runtime:** compositor socket name **`wayland-trierarch`** under the app-provided dir (e.g. `getFilesDir()/usr/tmp`). Clients set `XDG_RUNTIME_DIR` and `WAYLAND_DISPLAY=wayland-trierarch`.

## Prerequisites

- Android NDK (`ANDROID_NDK_HOME` or SDK’s `ndk/<version>`)
- For **manual** compositor-only build: `libwayland-server.so` and `libffi.so` for **arm64-v8a** plus headers under `libs/lib/` and `libs/include/` (this repo already ships `libs/include/`, `libs/share/`).
- For the **all-in-one script:** host packages **meson**, **ninja**, **wayland**, **wayland-protocols** (e.g. Arch: `pacman -S meson ninja wayland wayland-protocols`).

This module is **arm64-v8a only** (`Application.mk`).

## Manual build

Use this when you already have **`libs/lib/`** populated with `libwayland-server.so` and `libffi.so` (and headers). From **`trierarch-wayland/`**:

```bash
cd trierarch-wayland
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_ABI=arm64-v8a
```

**Intermediate output:** `obj/local/arm64-v8a/libwayland-compositor.so` — you still need the two dependency `.so` files next to it for packaging; the **recommended** workflow is the script below, which collects everything under `out/arm64-v8a/`.

## Script build (recommended)

Builds **libffi**, **Wayland**, and the compositor, then places **all** `.so` files in **`out/arm64-v8a/`**:

```bash
cd trierarch-wayland
./scripts/build-wayland-android.sh
```

Requires the host tools listed above. Set `ANDROID_NDK_HOME` if the NDK is not found automatically.

**Output:** `trierarch-wayland/out/arm64-v8a/libwayland-compositor.so`, `libwayland-server.so`, `libffi.so`.

## Using the build artifacts

Copy every `.so` from `out/arm64-v8a/` into the app’s JNI libs (from monorepo root):

```bash
cp trierarch-wayland/out/arm64-v8a/*.so trierarch-app/app/src/main/jniLibs/arm64-v8a/
```

The app loads **`wayland-compositor`** (`System.loadLibrary("wayland-compositor")`) and uses JNI on `app.trierarch.WaylandBridge` (`nativeStartServer`, `nativeSurfaceCreated`, `nativeSurfaceDestroyed`, `nativeStopWayland`, `nativeIsWaylandReady`, `nativeGetOutputSize`, `nativeGetSocketDir`, `nativeOnPointerEvent`, `nativeSetCursorPhysical`, `nativeOnKeyEvent`, `nativeCommitTextUtf8`, `nativeHasActiveClients`, …).

Then build the APK from `trierarch-app/`. See [`README_DEV.md`](../README_DEV.md).

## Repository layout (this directory)

- **`protocol/`** — generated protocol stubs used by the compositor.
- **`libs/`** — Wayland/libffi headers and (after script or manual steps) shared libs.
- **`out/arm64-v8a/`** — unified output for packaging into the APK.
- **`wayland/`**, **`wayland-protocols/`**, **`libffi/`** — upstream sources; keep their `COPYING` / `LICENSE` files.

License for **our** compositor sources: see the **root** [`LICENSE`](../LICENSE) of the monorepo.
