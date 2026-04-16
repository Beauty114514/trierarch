#!/bin/bash
# Build trierarch (Rust cdylib) for aarch64 Android; copy .so to app jniLibs.
# Requires: rustup target add aarch64-linux-android; NDK.
# Set ANDROID_NDK_HOME or NDK points to first available under $HOME/Android/Sdk/ndk/
set -e
cd "$(dirname "$0")/.."

# NDK: prefer ANDROID_NDK_HOME, then first NDK in Sdk (pick one dir; multiple versions break `echo * | head -1`)
NDK="${ANDROID_NDK_HOME:-${NDK:-}}"
if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then
    NDK_BASE="$HOME/Android/Sdk/ndk"
    if [ -d "$NDK_BASE" ]; then
        NDK=$(find "$NDK_BASE" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort -V | tail -1)
    fi
fi
[ -n "$NDK" ] && [ -d "$NDK" ] || { echo "Android NDK not found. Set ANDROID_NDK_HOME or install NDK."; exit 1; }

# Toolchain host: linux-x86_64 or darwin-x86_64 / darwin-arm64
UNAME_S=$(uname -s | tr '[:upper:]' '[:lower:]')
UNAME_M=$(uname -m)
case "$UNAME_M" in
  x86_64) TOOLCHAIN_ARCH="x86_64" ;;
  aarch64|arm64) TOOLCHAIN_ARCH="aarch64" ;;
  *) TOOLCHAIN_ARCH="x86_64" ;;
esac
NDK_HOST="${UNAME_S}-${TOOLCHAIN_ARCH}"
NDK_BIN="$NDK/toolchains/llvm/prebuilt/$NDK_HOST/bin"

export AR="$NDK_BIN/llvm-ar"
export CC="$NDK_BIN/aarch64-linux-android24-clang"
export CXX="$NDK_BIN/aarch64-linux-android24-clang++"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$NDK_BIN/aarch64-linux-android24-clang"
export CARGO_TARGET_DIR="$PWD/target"

rustup target add aarch64-linux-android 2>/dev/null || true
cargo build --release --target aarch64-linux-android --manifest-path Cargo.toml

JNI_LIBS_DIR="../trierarch-app/app/src/main/jniLibs/arm64-v8a"
mkdir -p "$JNI_LIBS_DIR"
cp -f target/aarch64-linux-android/release/libtrierarch.so "$JNI_LIBS_DIR/"
echo "Built and copied to $JNI_LIBS_DIR/libtrierarch.so"
