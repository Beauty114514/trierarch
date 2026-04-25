#!/bin/bash
# Build libffi, Wayland server, and xdg-shell protocol for Android arm64-v8a.
# Output: libs/ (for ndk-build), protocol/, and out/arm64-v8a/ (unified dir for app jniLibs).
#
# Dependencies (Arch): meson ninja wayland wayland-protocols
#   pacman -S meson ninja wayland wayland-protocols
#
# NDK: ANDROID_NDK_HOME or NDK, or first dir under $HOME/Android/Sdk/ndk/
# wayland-scanner: from PATH, or set WAYLAND_SCANNER

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$PROJECT_DIR/libs"
OUT_UNIFIED="$PROJECT_DIR/out/arm64-v8a"
FFI_INSTALL="$PROJECT_DIR/libffi-install"
BUILD_SRC_DIR="${WAYLAND_BUILD_SRC_DIR:-$PROJECT_DIR/build-src}"
WAYLAND_SRC="${WAYLAND_SRC:-$BUILD_SRC_DIR/wayland}"
PROTOCOLS_SRC="${WAYLAND_PROTOCOLS_SRC:-$BUILD_SRC_DIR/wayland-protocols}"

# NDK: prefer ANDROID_NDK_HOME, then NDK, then latest dir under Sdk/ndk/
NDK="${ANDROID_NDK_HOME:-${NDK:-}}"
if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then
    NDK_BASE="$HOME/Android/Sdk/ndk"
    if [ -d "$NDK_BASE" ]; then
        NDK=$(find "$NDK_BASE" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort -V | tail -1)
    fi
fi
[ -n "$NDK" ] && [ -d "$NDK" ] || { echo "Android NDK not found. Set ANDROID_NDK_HOME or install NDK under \$HOME/Android/Sdk/ndk/"; exit 1; }

# Toolchain host: linux-x86_64, darwin-x86_64, darwin-arm64 (match NDK prebuilt dir)
UNAME_S=$(uname -s | tr '[:upper:]' '[:lower:]')
UNAME_M=$(uname -m)
case "$UNAME_M" in
  x86_64|amd64) NDK_HOST="${UNAME_S}-x86_64" ;;
  aarch64|arm64) NDK_HOST="${UNAME_S}-arm64" ;;
  *) NDK_HOST="${UNAME_S}-x86_64" ;;
esac
[ -d "$NDK/toolchains/llvm/prebuilt/$NDK_HOST" ] || { echo "NDK prebuilt not found: $NDK/toolchains/llvm/prebuilt/$NDK_HOST"; exit 1; }

# wayland-scanner: from PATH or common locations
WAYLAND_SCANNER="${WAYLAND_SCANNER:-$(command -v wayland-scanner 2>/dev/null || true)}"
[ -z "$WAYLAND_SCANNER" ] && [ -x /usr/bin/wayland-scanner ] && WAYLAND_SCANNER=/usr/bin/wayland-scanner
[ -n "$WAYLAND_SCANNER" ] && [ -x "$WAYLAND_SCANNER" ] || { echo "wayland-scanner not found. Install wayland (e.g. pacman -S wayland) or set WAYLAND_SCANNER."; exit 1; }
export WAYLAND_SCANNER

CROSS_FILE="/tmp/cross-android-$$.txt"
trap "rm -f $CROSS_FILE" EXIT
sed -e "s|@NDK@|$NDK|g" -e "s|@NDK_HOST@|$NDK_HOST|g" "$SCRIPT_DIR/cross-android-arm64.txt" > "$CROSS_FILE"

# 1. Build libffi (Wayland depends on it; use autotools)
FFI_VERSION="3.4.6"
FFI_SRC="$PROJECT_DIR/libffi"
FFI_TAR="$PROJECT_DIR/libffi-${FFI_VERSION}.tar.gz"
if [ -d "$FFI_SRC" ] && { [ ! -f "$FFI_SRC/.ffi-version" ] || [ "$(cat "$FFI_SRC/.ffi-version" 2>/dev/null)" != "$FFI_VERSION" ]; }; then
    echo "Upgrading libffi to $FFI_VERSION, removing old..."
    rm -rf "$FFI_SRC"
fi
if [ ! -f "$FFI_TAR" ]; then
    echo "Downloading libffi $FFI_VERSION..."
    curl -sL "https://github.com/libffi/libffi/releases/download/v${FFI_VERSION}/libffi-${FFI_VERSION}.tar.gz" -o "$FFI_TAR"
fi
if [ ! -d "$FFI_SRC" ] || [ ! -f "$FFI_SRC/configure" ]; then
    echo "Extracting libffi..."
    rm -rf "$FFI_SRC"
    tar xzf "$FFI_TAR" -C "$PROJECT_DIR"
    mv "$PROJECT_DIR/libffi-${FFI_VERSION}" "$FFI_SRC"
    echo "$FFI_VERSION" > "$FFI_SRC/.ffi-version"
fi
echo "Building libffi for Android..."
rm -rf "$FFI_INSTALL" "$FFI_SRC/build-android"
mkdir -p "$FFI_INSTALL" "$FFI_SRC/build-android"
(cd "$FFI_SRC/build-android" && \
 CC="$NDK/toolchains/llvm/prebuilt/$NDK_HOST/bin/aarch64-linux-android23-clang" \
 CFLAGS="-DANDROID -fPIC -std=gnu11" \
 LDFLAGS="-fPIC" \
 "$FFI_SRC/configure" --host=aarch64-linux-android --prefix="$FFI_INSTALL" \
    --disable-docs --disable-multi-os-directory --disable-static)
make -C "$FFI_SRC/build-android" -j$(nproc)
make -C "$FFI_SRC/build-android" install
export PKG_CONFIG_PATH="$FFI_INSTALL/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

# 2. Clone Wayland (default: match host wayland-scanner)
mkdir -p "$BUILD_SRC_DIR"
if [ -d "$WAYLAND_SRC" ]; then
    echo "Removing existing wayland, re-cloning..."
    rm -rf "$WAYLAND_SRC"
fi

HOST_WAYLAND_SCANNER_VER="$(pkg-config --modversion wayland-scanner 2>/dev/null || true)"
WAYLAND_VERSION="${WAYLAND_VERSION:-${HOST_WAYLAND_SCANNER_VER:-1.25.0}}"
echo "Cloning wayland $WAYLAND_VERSION..."
git clone --depth 1 --branch "$WAYLAND_VERSION" https://gitlab.freedesktop.org/wayland/wayland.git "$WAYLAND_SRC"
rm -rf "$WAYLAND_SRC/.git"

if [ ! -d "$PROTOCOLS_SRC" ]; then
    echo "Cloning wayland-protocols..."
    git clone --depth 1 https://gitlab.freedesktop.org/wayland/wayland-protocols.git "$PROTOCOLS_SRC"
    rm -rf "$PROTOCOLS_SRC/.git"
fi

# 3. Build Wayland (server lib only)
echo "Building wayland..."
mkdir -p "$WAYLAND_SRC/build-android"

meson setup "$WAYLAND_SRC/build-android" "$WAYLAND_SRC" \
    --cross-file "$CROSS_FILE" \
    -Dlibraries=true \
    -Dscanner=false \
    -Dtests=false \
    -Ddocumentation=false \
    -Ddtd_validation=false \
    --prefix "$OUT_DIR" \
    --libdir lib

meson compile -C "$WAYLAND_SRC/build-android"
meson install -C "$WAYLAND_SRC/build-android"

# Copy libffi into libs (wayland-server needs it at runtime)
mkdir -p "$OUT_DIR/lib" "$OUT_DIR/include"
cp -a "$FFI_INSTALL/lib"/libffi.so* "$OUT_DIR/lib/" 2>/dev/null || true
cp -a "$FFI_INSTALL/include"/ffi*.h "$OUT_DIR/include/" 2>/dev/null || true

# Copy app-needed .so to unified output (for copying to trierarch-app jniLibs)
rm -rf "$OUT_UNIFIED"
mkdir -p "$OUT_UNIFIED"
cp -a "$OUT_DIR/lib"/libwayland-server.so "$OUT_DIR/lib"/libffi.so* "$OUT_UNIFIED/" 2>/dev/null || true

# 4. Generate xdg-shell server protocol (used by this compositor)
XDG_XML="$PROTOCOLS_SRC/stable/xdg-shell/xdg-shell.xml"
FRACTIONAL_XML="$PROTOCOLS_SRC/staging/fractional-scale/fractional-scale-v1.xml"
ANDROID_WLEGL_XML="$PROJECT_DIR/protocol/xml/android-wlegl.xml"
PROTO_DIR="$PROJECT_DIR/protocol"
mkdir -p "$PROTO_DIR"
if [ -f "$XDG_XML" ]; then
    "$WAYLAND_SCANNER" server-header "$XDG_XML" "$PROTO_DIR/xdg-shell-server-protocol.h"
    "$WAYLAND_SCANNER" private-code "$XDG_XML" "$PROTO_DIR/xdg-shell-server-protocol.c"
    echo "Generated xdg-shell protocol in $PROTO_DIR"
fi
if [ -f "$FRACTIONAL_XML" ]; then
    "$WAYLAND_SCANNER" server-header "$FRACTIONAL_XML" "$PROTO_DIR/fractional-scale-v1-server-protocol.h"
    "$WAYLAND_SCANNER" private-code "$FRACTIONAL_XML" "$PROTO_DIR/fractional-scale-v1-server-protocol.c"
    echo "Generated fractional-scale-v1 protocol in $PROTO_DIR"
fi
if [ -f "$ANDROID_WLEGL_XML" ]; then
    "$WAYLAND_SCANNER" server-header "$ANDROID_WLEGL_XML" "$PROTO_DIR/android-wlegl-server-protocol.h"
    "$WAYLAND_SCANNER" private-code "$ANDROID_WLEGL_XML" "$PROTO_DIR/android-wlegl-server-protocol.c"
    echo "Generated android-wlegl protocol in $PROTO_DIR"
fi

# 5. Build compositor and copy to unified output (use same NDK as above)
NDK_BUILD="$NDK/ndk-build"
if [ ! -x "$NDK_BUILD" ]; then
    echo "ndk-build not found at $NDK_BUILD; skip compositor. Copy libwayland-compositor.so to $OUT_UNIFIED manually after building."
else
    echo "Building libwayland-compositor.so..."
    if (cd "$PROJECT_DIR" && "$NDK_BUILD" \
        NDK_PROJECT_PATH=. \
        APP_BUILD_SCRIPT=./Android.mk \
        NDK_APPLICATION_MK=./Application.mk \
        APP_PLATFORM=android-26 \
        APP_ABI=arm64-v8a \
        -j"$(nproc)"); then
        COMPOSITOR_SRC="$PROJECT_DIR/obj/local/arm64-v8a/libwayland-compositor.so"
        [ ! -f "$COMPOSITOR_SRC" ] && COMPOSITOR_SRC="$PROJECT_DIR/libs/arm64-v8a/libwayland-compositor.so"
        if [ -f "$COMPOSITOR_SRC" ]; then
            cp -a "$COMPOSITOR_SRC" "$OUT_UNIFIED/"
            echo "Installed libwayland-compositor.so to $OUT_UNIFIED"
        fi
    else
        echo "ndk-build failed; see above. Copy libwayland-compositor.so to $OUT_UNIFIED manually."
    fi
fi

echo ""
echo "Done. Wayland libs at: $OUT_DIR"
ls -la "$OUT_DIR/lib/"
echo ""
echo "Unified output (for app jniLibs): $OUT_UNIFIED"
ls -la "$OUT_UNIFIED/" 2>/dev/null || true
echo ""
echo "External upstream sources (re-cloned by this script): $BUILD_SRC_DIR"
echo ""
echo "Copy all .so from $OUT_UNIFIED to trierarch-app/app/src/main/jniLibs/arm64-v8a/"
