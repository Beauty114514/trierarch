#!/usr/bin/env bash
# One-shot: clone Pulse v17.0 (if needed), integrate trierarch-audio/pulse-android, build a
# minimal aarch64 Android prefix via Meson, install to trierarch-audio/out/pulse-android-prefix/.
#
# Requires: git, curl, meson, ninja, cmake, pkg-config.
# NDK: ANDROID_NDK_HOME or NDK, or first $HOME/Android/Sdk/ndk/* (same as trierarch-native).
#
# Env (optional):
#   PULSE_SRC / PULSE_CLONE_DIR — existing or alternate checkout (see below)
#   PULSE_SYSROOT — deps prefix (default: trierarch-audio/third_party/pulse-sysroot)
#   PULSE_INSTALL_PREFIX — meson --prefix (default: trierarch-audio/out/pulse-android-prefix)
#   PULSE_SKIP_SYSROOT=1 — reuse sysroot only if already built; fail if incomplete
#   PULSE_REBUILD_SYSROOT=1 — rebuild glob/execinfo/sndfile/ltdl
#   PULSE_SKIP_MESON=1 — stop after integrate
#   PULSE_SKIP_INSTALL_TO_APP=1 — do not rsync/cp prefix into trierarch-app assets after install
#   PULSE_APP_ASSETS_DIR — override destination (default: ../trierarch-app/app/src/main/assets/pulse)
#   PULSE_API — Android API for clang (default: 26, for AAudio)
#   FORCE_INTEGRATE=1 — reset git tree and re-apply patches (for build-pulse.sh)
set -euo pipefail

AUDIO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ANDROID_DIR="$AUDIO_ROOT/build-android"
API="${PULSE_API:-26}"
SYSROOT="${PULSE_SYSROOT:-$AUDIO_ROOT/third_party/pulse-sysroot}"
PREFIX="${PULSE_INSTALL_PREFIX:-$AUDIO_ROOT/out/pulse-android-prefix}"
MESON_BUILD="${PULSE_MESON_BUILD:-}" # default: $PULSE_SRC/trierarch-meson

# --- NDK (match trierarch-native/scripts/build_rust.sh) ---
NDK="${ANDROID_NDK_HOME:-${NDK:-}}"
if [[ -n "$NDK" && ! -d "$NDK" ]]; then
  NDK=""
fi
if [[ -z "$NDK" ]]; then
  NDK=$(echo "$HOME/Android/Sdk/ndk/"* 2>/dev/null | head -1)
fi
if [[ ! -d "$NDK" ]]; then
  echo "Android NDK not found. Set ANDROID_NDK_HOME."
  exit 1
fi

UNAME_S=$(uname -s | tr '[:upper:]' '[:lower:]')
UNAME_M=$(uname -m)
case "$UNAME_M" in
  x86_64|amd64) TOOLCHAIN_ARCH="x86_64" ;;
  aarch64|arm64) TOOLCHAIN_ARCH="aarch64" ;;
  *) TOOLCHAIN_ARCH="x86_64" ;;
esac
NDK_HOST="${UNAME_S}-${TOOLCHAIN_ARCH}"
NDK_BIN="$NDK/toolchains/llvm/prebuilt/$NDK_HOST/bin"
if [[ ! -x "$NDK_BIN/aarch64-linux-android${API}-clang" ]]; then
  echo "NDK clang not found: $NDK_BIN/aarch64-linux-android${API}-clang"
  exit 1
fi

# --- Pulse source tree ---
if [[ -n "${PULSE_SRC:-}" ]]; then
  DEST="$PULSE_SRC"
else
  DEST="${PULSE_CLONE_DIR:-$AUDIO_ROOT/third_party/pulseaudio}"
fi

if [[ ! -f "$DEST/meson.build" ]]; then
  echo "Cloning PulseAudio v17.0 -> $DEST"
  mkdir -p "$(dirname "$DEST")"
  git clone --depth 1 --branch v17.0 https://github.com/pulseaudio/pulseaudio.git "$DEST"
fi

export PULSE_SRC="$DEST"

run_integrate() {
  bash "$BUILD_ANDROID_DIR/build-pulse.sh"
}

fetch_raw() {
  local url=$1 dest=$2
  if [[ -f "$dest" ]]; then
    return 0
  fi
  mkdir -p "$(dirname "$dest")"
  curl -fsSL "$url" -o "$dest"
}

ensure_sysroot() {
  local MARK="$SYSROOT/.trierarch-pulse-sysroot"
  if [[ -n "${PULSE_REBUILD_SYSROOT:-}" ]]; then
    rm -f "$MARK"
  fi
  if [[ "${PULSE_SKIP_SYSROOT:-0}" == "1" ]]; then
    if [[ ! -f "$SYSROOT/lib/pkgconfig/sndfile.pc" ]]; then
      echo "PULSE_SKIP_SYSROOT=1 but $SYSROOT has no lib/pkgconfig/sndfile.pc"
      exit 1
    fi
    return 0
  fi
  if [[ -f "$MARK" ]]; then
    return 0
  fi

  echo "=== trierarch-audio: building pulse sysroot -> $SYSROOT ==="
  mkdir -p "$SYSROOT"/{lib/pkgconfig,include,bin}

  local CC="$NDK_BIN/aarch64-linux-android${API}-clang"
  local AR="$NDK_BIN/llvm-ar"
  local WORK="$AUDIO_ROOT/third_party/.pulse-deps-work"
  mkdir -p "$WORK"
  local TB="https://raw.githubusercontent.com/termux/termux-packages/master/packages"

  # libandroid-glob
  mkdir -p "$WORK/glob"
  fetch_raw "$TB/libandroid-glob/glob.c" "$WORK/glob/glob.c"
  fetch_raw "$TB/libandroid-glob/glob.h" "$WORK/glob/glob.h"
  # NDK <glob.h> lacks GLOB_* extensions; Termux glob.h must beat sysroot (needs -I before <glob.h>).
  "$CC" -fPIC -O2 -I"$WORK/glob" -c "$WORK/glob/glob.c" -o "$WORK/glob/glob.o"
  "$CC" -shared "$WORK/glob/glob.o" -o "$WORK/glob/libandroid-glob.so"
  "$AR" rcs "$WORK/glob/libandroid-glob.a" "$WORK/glob/glob.o"
  cp -f "$WORK/glob/libandroid-glob.so" "$WORK/glob/libandroid-glob.a" "$SYSROOT/lib/"
  cp -f "$WORK/glob/glob.h" "$SYSROOT/include/"

  # libandroid-execinfo
  mkdir -p "$WORK/execinfo"
  fetch_raw "$TB/libandroid-execinfo/execinfo.c" "$WORK/execinfo/execinfo.c"
  fetch_raw "$TB/libandroid-execinfo/execinfo.h" "$WORK/execinfo/execinfo.h"
  # Bionic <paths.h> has no _PATH_TMP; AOSP execinfo.c expects it for mkstemp template.
  "$CC" -fPIC -O2 -I"$WORK/execinfo" '-D_PATH_TMP="/tmp/"' \
    -c "$WORK/execinfo/execinfo.c" -o "$WORK/execinfo/execinfo.o"
  "$CC" -shared "$WORK/execinfo/execinfo.o" -o "$WORK/execinfo/libandroid-execinfo.so" \
    -Wl,-soname,libandroid-execinfo.so
  "$AR" rcs "$WORK/execinfo/libandroid-execinfo.a" "$WORK/execinfo/execinfo.o"
  cp -f "$WORK/execinfo/libandroid-execinfo.so" "$WORK/execinfo/libandroid-execinfo.a" "$SYSROOT/lib/"
  cp -f "$WORK/execinfo/execinfo.h" "$SYSROOT/include/"
  ln -sfn libandroid-execinfo.so "$SYSROOT/lib/libexecinfo.so"

  # libsndfile (CMake + NDK toolchain)
  local SF_VER=1.2.2
  local SF_DIR="$WORK/libsndfile-$SF_VER"
  if [[ ! -d "$SF_DIR" ]]; then
    fetch_raw "https://github.com/libsndfile/libsndfile/releases/download/${SF_VER}/libsndfile-${SF_VER}.tar.xz" \
      "$WORK/libsndfile-${SF_VER}.tar.xz"
    tar -xJf "$WORK/libsndfile-${SF_VER}.tar.xz" -C "$WORK"
  fi
  local SF_BUILD="$SF_DIR/build-android"
  rm -rf "$SF_BUILD"
  cmake -S "$SF_DIR" -B "$SF_BUILD" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-"$API" \
    -DCMAKE_INSTALL_PREFIX="$SYSROOT" \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_PROGRAMS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DENABLE_EXTERNAL_LIBS=OFF
  cmake --build "$SF_BUILD" -j"$(nproc 2>/dev/null || echo 4)"
  cmake --install "$SF_BUILD"

  # libltdl (GNU libtool)
  local LT_VER=2.4.7
  local LT_DIR="$WORK/libtool-$LT_VER"
  if [[ ! -d "$LT_DIR" ]]; then
    fetch_raw "https://ftpmirror.gnu.org/libtool/libtool-${LT_VER}.tar.gz" "$WORK/libtool-${LT_VER}.tar.gz"
    tar -xzf "$WORK/libtool-${LT_VER}.tar.gz" -C "$WORK"
  fi
  (
    cd "$LT_DIR"
    if [[ ! -f Makefile ]]; then
      ./configure --host=aarch64-linux-android --prefix="$SYSROOT" \
        CC="$CC" AR="$AR" LT_SYS_LIBRARY_PATH="$SYSROOT/lib" \
        CFLAGS="-fPIC -O2 -I$SYSROOT/include" LDFLAGS="-L$SYSROOT/lib" \
        --enable-shared --disable-static
    fi
    make -j"$(nproc 2>/dev/null || echo 4)"
    make install
  )

  touch "$MARK"
  echo "=== sysroot ready ==="
}

run_meson() {
  local CROSS_OUT="/tmp/trierarch-pulse-cross-$$.txt"
  sed -e "s|@NDK@|$NDK|g" -e "s|@NDK_HOST@|$NDK_HOST|g" \
    -e "s|@SYSROOT@|$SYSROOT|g" -e "s|@API@|$API|g" \
    "$BUILD_ANDROID_DIR/cross-pulse-android-arm64.txt.in" >"$CROSS_OUT"

  export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
  export PKG_CONFIG_PATH="$SYSROOT/lib/pkgconfig"
  export PKG_CONFIG_LIBDIR="$SYSROOT/lib/pkgconfig"

  local BUILD="${MESON_BUILD:-$PULSE_SRC/trierarch-meson}"
  rm -rf "$BUILD"
  meson setup "$BUILD" "$PULSE_SRC" \
    --cross-file "$CROSS_OUT" \
    --prefix="$PREFIX" \
    -Dalsa=disabled \
    -Dx11=disabled \
    -Dgtk=disabled \
    -Dopenssl=disabled \
    -Dgsettings=disabled \
    -Ddoxygen=false \
    -Ddatabase=simple \
    -Ddbus=disabled \
    -Dsystemd=disabled \
    -Dbluez5=disabled \
    -Dudev=disabled \
    -Dtests=false \
    -Dman=false \
    -Dspeex=disabled \
    -Dsoxr=disabled \
    -Dwebrtc-aec=disabled \
    -Dadrian-aec=true \
    -Dglib=disabled \
    -Dasyncns=disabled \
    -Dgstreamer=disabled \
    -Djack=disabled \
    -Davahi=disabled \
    -Dtcpwrap=disabled \
    -Dlirc=disabled \
    -Dfftw=disabled \
    -Dorc=disabled \
    -Dsamplerate=disabled \
    -Dipv6=true
  meson compile -C "$BUILD"
  meson install -C "$BUILD"
  rm -f "$CROSS_OUT"

  # Copy sysroot runtime deps into the install prefix so they ship with the APK.
  # The pulseaudio binary / libs link against these but Meson only installs its own outputs.
  echo "Copying sysroot runtime deps into prefix..."
  for lib in libltdl.so libsndfile.so libandroid-execinfo.so libandroid-glob.so libexecinfo.so; do
    if [[ -f "$SYSROOT/lib/$lib" ]]; then
      cp -f "$SYSROOT/lib/$lib" "$PREFIX/lib/"
      echo "  $lib"
    fi
  done

  echo ""
  echo "Installed Pulse prefix -> $PREFIX"
  install_pulse_to_app_assets
}

# Sync install prefix into app APK assets; at runtime PulseAssets copies to filesDir/pulse/.
install_pulse_to_app_assets() {
  if [[ "${PULSE_SKIP_INSTALL_TO_APP:-0}" == "1" ]]; then
    echo "PULSE_SKIP_INSTALL_TO_APP=1: not syncing prefix to app assets."
    return
  fi
  local APP_ASSETS="${PULSE_APP_ASSETS_DIR:-$AUDIO_ROOT/../trierarch-app/app/src/main/assets/pulse}"
  if [[ ! -d "$PREFIX/bin" ]]; then
    echo "install_pulse_to_app_assets: incomplete prefix ($PREFIX), skip."
    return
  fi
  date -u +"%Y-%m-%dT%H:%M:%SZ" >"$PREFIX/.pack_stamp"
  mkdir -p "$APP_ASSETS"
  if command -v rsync >/dev/null 2>&1; then
    rsync -a --delete "$PREFIX/" "$APP_ASSETS/"
  else
    rm -rf "$APP_ASSETS"
    mkdir -p "$APP_ASSETS"
    cp -a "$PREFIX/." "$APP_ASSETS/"
  fi
  echo "Synced prefix -> $APP_ASSETS (packaged as assets/pulse; app extracts to filesDir/pulse)"
}

run_integrate

if [[ "${PULSE_SKIP_MESON:-0}" == "1" ]]; then
  echo "PULSE_SKIP_MESON=1: skipping sysroot and Meson."
  exit 0
fi

ensure_sysroot
run_meson
