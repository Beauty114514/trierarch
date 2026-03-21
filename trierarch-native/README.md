# trierarch-native

[中文](README.zh.md) | English

---

Rust JNI library for [Trierarch](https://github.com/Beauty114514/trierarch-app): proot launch, PTY I/O, and Arch rootfs download/extract. Used by the Android app via `NativeBridge`.

## Prerequisites

- Rust (rustup)
- Android NDK (`ANDROID_NDK_HOME` or under `$HOME/Android/Sdk/ndk/`)

```bash
rustup target add aarch64-linux-android
```

## Build

From this repo root, set the NDK toolchain and build:

```bash
export NDK="${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/25.2.9519653}"   # or your NDK path
export AR="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
export CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang"
export CXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang++"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$CC"

cargo build --release --target aarch64-linux-android
```

On macOS use `darwin-x86_64` or `darwin-arm64` instead of `linux-x86_64` in the paths.

**Output:** `target/aarch64-linux-android/release/libtrierarch.so`

Copy this file into the app’s jniLibs so the Android app can load it:

```bash
cp target/aarch64-linux-android/release/libtrierarch.so /path/to/trierarch-app/app/src/main/jniLibs/arm64-v8a/
```

## Optional: script

`scripts/build_rust.sh` is intended for a workspace that contains both this crate and the app. When using this repo alone, use the `cargo build` and `cp` steps above.
