## trierarch-virgl

This module builds a host-side VirGL vtest server for Android and packages it into the app.

### What it produces

- `virgl_test_server_android` (host binary)

### Where it goes

The build script copies the binary into:

- `trierarch-app/app/src/main/assets/virgl/arm64-v8a/virgl_test_server_android`

At runtime, the app extracts it to `files/virgl/bin/virgl_test_server_android` and the native layer
spawns it when `Renderer` is set to `UNIVERSAL`.

### Build (scripted)

See `build-android/build-virgl-android.sh`.

### Upstream sources (Termux-derived)

This follows the same high-level approach as Termux `virglrenderer-android`:

- build `libepoxy` (EGL-only) + `virglrenderer` (EGL platform)
- rename `virgl_test_server` → `virgl_test_server_android`

Reference: `termux/termux-packages` `packages/virglrenderer-android/build.sh`.

