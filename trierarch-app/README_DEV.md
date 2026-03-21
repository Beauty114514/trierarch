# Trierarch (App) - Developer Docs

[User docs](README.md) | English

---

This document is meant for developers who want to build the Android APK from source.

## Prerequisites

- Android Studio (or SDK + NDK)
- Built artifacts from **trierarch-proot** (`libproot.so`, `libproot_loader.so`) and **trierarch-native** (`libtrierarch.so`).
- To use the built-in Wayland view: see the section below.

## Wayland GUI (trierarch-wayland)

To enable the in-app Wayland compositor UI:

1. Build **trierarch-wayland**.
2. Copy all `.so` files from `out/arm64-v8a/` into `app/src/main/jniLibs/arm64-v8a/`.
3. Ensure `app/src/main/assets/keymap_us.xkb` exists in the APK (bundled with this repo).

## Build

1. Get proot libs from **trierarch-proot** (see that repo’s README). Copy into this repo:

   ```bash
   mkdir -p app/src/main/jniLibs/arm64-v8a
   cp /path/to/trierarch-proot/build-android/out/aarch64/proot   app/src/main/jniLibs/arm64-v8a/libproot.so
   cp /path/to/trierarch-proot/build-android/out/aarch64/loader  app/src/main/jniLibs/arm64-v8a/libproot_loader.so
   ```

2. Get `libtrierarch.so` from **trierarch-native** (see that repo’s README). Copy into this repo:

   ```bash
   cp /path/to/trierarch-native/target/aarch64-linux-android/release/libtrierarch.so app/src/main/jniLibs/arm64-v8a/
   ```

3. Build the APK from this repo root:

   ```bash
   ./gradlew assembleDebug
   ./gradlew installDebug
   ```

Output: `app/build/outputs/apk/debug/app-debug.apk`.

## Notes

- The app stores its Arch rootfs under `data_dir/arch` (app internal storage). If missing at first launch, it will be auto-downloaded via `proot-distro` (network required).

