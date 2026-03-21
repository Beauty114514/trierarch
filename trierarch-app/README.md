# Android application

[中文](README.zh.md) | English

Part of the [Trierarch monorepo](https://github.com/Beauty114514/trierarch).

## Role and what’s implemented

- **Jetpack Compose** UI: install flow, terminal, side menu, Wayland view toggle, Display startup script editor, settings.
- **Assets:** proot launcher script, keymap, bundled JNI names; **`jniLibs/arm64-v8a/`** loads `.so` files built in other directories (Rust JNI, proot, Wayland stack).
- **Bridges:** Kotlin `NativeBridge` / `WaylandBridge` call into native code.

This directory does **not** build those native libraries; see [`README_DEV.md`](../README_DEV.md) and each component’s README.

## Prerequisites

- Android Studio or Android SDK + a compatible **NDK** (for Gradle / prefab if used).
- Prebuilt `.so` dependencies copied into `app/src/main/jniLibs/arm64-v8a/` as described in [`README_DEV.md`](../README_DEV.md).

## Manual build (Gradle)

From **`trierarch-app/`** (monorepo layout):

```bash
cd trierarch-app
./gradlew assembleDebug
```

Optional install to a connected device:

```bash
./gradlew installDebug
```

**Output (example):** `app/build/outputs/apk/debug/app-debug.apk`.

There is no separate wrapper script beyond **`gradlew`**; that is the standard build entry.

## Using the build output

- Install the APK on an **arm64-v8a** device/emulator.
- At runtime the app expects native libs under `jniLibs` and optional Arch rootfs under app storage (see root [`README.md`](../README.md)).

For assembling JNI libs before `./gradlew`, follow [`README_DEV.md`](../README_DEV.md).
