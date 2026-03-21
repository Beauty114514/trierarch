# trierarch-native

[中文](README.zh.md) | English

Part of the [Trierarch monorepo](https://github.com/Beauty114514/trierarch).

## Role and what’s implemented

Rust **`cdylib`** loaded by the Android app as **`libtrierarch.so`** (`System.loadLibrary("trierarch")`). JNI entry points are used from `NativeBridge`.

**Implemented (high level):** proot-related process/PTY handling, Arch rootfs download with checksum verification, staged extract and atomic install into app storage, PTY output buffering, and other helpers the Kotlin layer calls.

## Prerequisites

- [Rust](https://www.rust-lang.org/) via `rustup`
- Android NDK (`ANDROID_NDK_HOME` or under `$HOME/Android/Sdk/ndk/`)

```bash
rustup target add aarch64-linux-android
```

## Manual build

From **`trierarch-native/`**, point Cargo at the NDK LLVM toolchain (adjust `linux-x86_64` to `darwin-x86_64` / `darwin-arm64` on macOS):

```bash
cd trierarch-native
export NDK="${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/25.2.9519653}"   # your NDK
export AR="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
export CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang"
export CXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang++"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$CC"

cargo build --release --target aarch64-linux-android
```

**Output:** `target/aarch64-linux-android/release/libtrierarch.so`

## Script build

From **`trierarch-native/`**:

```bash
./scripts/build_rust.sh
```

The script discovers the NDK, exports the same toolchain variables, runs `cargo build --release --target aarch64-linux-android`, then copies:

`target/aarch64-linux-android/release/libtrierarch.so` → `../trierarch-app/app/src/main/jniLibs/arm64-v8a/`

**Requires** the standard monorepo layout (`trierarch-app` next to `trierarch-native`).

## Using the build artifact

If you built manually, copy the library next to the app module:

```bash
cp target/aarch64-linux-android/release/libtrierarch.so ../trierarch-app/app/src/main/jniLibs/arm64-v8a/
```

Then build the APK from `trierarch-app/` (see [`README_DEV.md`](../README_DEV.md)).
