# trierarch-proot

[中文](README.zh.md) | English

Part of the [Trierarch monorepo](https://github.com/Beauty114514/trierarch). Based on [termux/proot](https://github.com/termux/proot). Upstream license/attribution: see **`COPYING`** in this directory.

## Role and what’s implemented

Cross-compile **PRoot** and its **loader** for **Android arm64-v8a**. The Android app loads them as native libraries (`libproot.so`, `libproot_loader.so`, optionally loader32) to run the Arch rootfs environment.

## Prerequisites

- Android NDK (`ANDROID_NDK_HOME` or under `$HOME/Android/Sdk/ndk/`)
- Host tools for local build: `make`, `wget`, `tar`, `bash`
- **Docker** (optional, for the scripted container build)

## Manual build

From **`trierarch-proot/build-android/`**:

```bash
cd trierarch-proot/build-android
./get-talloc.sh
./make-talloc-static.sh
./make-proot.sh
```

Set `NDK=/path/to/ndk` if your NDK is not in the default location expected by the scripts.

**Output:** `trierarch-proot/build-android/out/aarch64/proot`, `loader`, and optionally `loader32`.

## Script build (Docker)

From the **monorepo root** (recommended when you want the same environment as CI / avoid host Python 2 issues):

```bash
./trierarch-proot/build-android/docker-build.sh
# or:
./trierarch-proot/build-android/docker-build.sh /path/to/ndk
```

This builds the Docker image under `build-android/`, mounts the **whole monorepo** at `/workspace`, runs `./build.sh` inside **`trierarch-proot/build-android`**, and writes binaries to `trierarch-proot/build-android/out/aarch64/`.

Alternative (equivalent to what the script does):

```bash
cd trierarch-proot
NDK="${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/25.2.9519653}"
docker build -t trierarch-proot-build build-android
docker run --rm -v "$PWD/..:/workspace" -v "$NDK:/opt/ndk" -e NDK=/opt/ndk \
  -w /workspace/trierarch-proot/build-android trierarch-proot-build ./build.sh
```

(Adjust `-v` so the monorepo root is `/workspace`.)

## Using the build artifacts

Rename/copy into the app’s JNI directory (paths from **monorepo root**):

```bash
mkdir -p trierarch-app/app/src/main/jniLibs/arm64-v8a
cp trierarch-proot/build-android/out/aarch64/proot   trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot.so
cp trierarch-proot/build-android/out/aarch64/loader trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot_loader.so
# if present:
# cp trierarch-proot/build-android/out/aarch64/loader32 trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot_loader32.so
```

Then build the APK from `trierarch-app/`. See [`README_DEV.md`](../README_DEV.md).
