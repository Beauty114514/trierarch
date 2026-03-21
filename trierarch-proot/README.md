# trierarch-proot

[中文](README.zh.md) | English

---

Proot built for Android (aarch64) for use by [Trierarch](https://github.com/your-org/trierarch-app). Based on [termux/proot](https://github.com/termux/proot) (patched PRoot for Termux).

## Prerequisites

- Android NDK (`ANDROID_NDK_HOME` or under `$HOME/Android/Sdk/ndk/`)
- [Docker](https://docs.docker.com/get-docker/) (optional; for the scripted build)

## Build

### Option 1: Docker (recommended)

From this repo root:

```bash
NDK="${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/25.2.9519653}"   # or your NDK path
docker build -t trierarch-proot-build build-android
docker run --rm -v "$PWD:/workspace" -v "$NDK:/opt/ndk" -e NDK=/opt/ndk -w /workspace/build-android trierarch-proot-build ./build.sh
```

Or with a custom NDK path:

```bash
./build-android/docker-build.sh /path/to/ndk
```

(If `docker-build.sh` is written for a parent repo layout, use the `docker build` + `docker run` commands above.)

**Output:** `build-android/out/aarch64/proot`, `build-android/out/aarch64/loader`.

### Option 2: Local

From `build-android/`:

```bash
cd build-android
./get-talloc.sh
./make-talloc-static.sh
./make-proot.sh
```

Requires: NDK, make, wget, tar, bash. Set `NDK=/path` if not using the default.

## Use in the app

Copy the built binaries into the trierarch-app repo as native libs (the app loads them from its lib directory):

```bash
mkdir -p /path/to/trierarch-app/app/src/main/jniLibs/arm64-v8a
cp build-android/out/aarch64/proot   /path/to/trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot.so
cp build-android/out/aarch64/loader  /path/to/trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot_loader.so
```
