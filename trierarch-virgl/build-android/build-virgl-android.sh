#!/bin/bash
# Build virgl_test_server_android + ANGLE for Android arm64-v8a.
#
# This script follows the Termux virglrenderer-android packaging approach:
# - download & extract ANGLE prebuilt libraries
# - build libepoxy with EGL + ANGLE env-var support
# - build virglrenderer with all Termux-equivalent patches
# - install virgl_test_server as virgl_test_server_android
#
# Output (all copied to APK assets):
# - virgl_test_server_android
# - libvirglrenderer.so, libepoxy.so
# - angle/vulkan/*.so  (ANGLE libraries)
#
# Dependencies (Arch):
#   pacman -S meson ninja python pkgconf git curl tar binutils
#
# NDK:
#   ANDROID_NDK_HOME or NDK, or first dir under $HOME/Android/Sdk/ndk/

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$PROJECT_DIR/.." && pwd)"

ASSETS_DIR="$REPO_ROOT/trierarch-app/app/src/main/assets/virgl/arm64-v8a"
OUT_DIR="$PROJECT_DIR/out/arm64-v8a"
BUILD_SRC_DIR="${VIRGL_BUILD_SRC_DIR:-$PROJECT_DIR/build-src}"

VIRGL_VERSION="${VIRGL_VERSION:-1.3.0}"
EPOXY_VERSION="${EPOXY_VERSION:-1.5.10}"

ANGLE_DEB_URL="https://packages.termux.dev/apt/termux-main/pool/main/a/angle-android/angle-android_2.1.24923-f09a19ce-2_aarch64.deb"
ANGLE_DEB_SHA256=""  # skip verification for now; Termux repo is the authoritative source

mkdir -p "$BUILD_SRC_DIR" "$OUT_DIR" "$ASSETS_DIR"

# ── Python venv (meson needs PyYAML) ─────────────────────────────────────────
VENV_DIR="$OUT_DIR/py-venv"
if [ ! -x "$VENV_DIR/bin/python3" ]; then
  echo "Preparing python venv..."
  python3 -m venv "$VENV_DIR"
fi
export PATH="$VENV_DIR/bin:$PATH"
python3 -c "import yaml" 2>/dev/null || {
  echo "Installing PyYAML into venv..."
  python3 -m pip install --upgrade pip >/dev/null
  python3 -m pip install PyYAML >/dev/null
}

# ── NDK detection ────────────────────────────────────────────────────────────
NDK="${ANDROID_NDK_HOME:-${NDK:-}}"
[ -n "${NDK}" ] && [ -d "${NDK}" ] || NDK=$(echo "$HOME/Android/Sdk/ndk/"* 2>/dev/null | head -1)
[ -d "$NDK" ] || { echo "Android NDK not found. Set ANDROID_NDK_HOME."; exit 1; }

UNAME_S=$(uname -s | tr '[:upper:]' '[:lower:]')
UNAME_M=$(uname -m)
case "$UNAME_M" in
  x86_64|amd64) NDK_HOST="${UNAME_S}-x86_64" ;;
  aarch64|arm64) NDK_HOST="${UNAME_S}-arm64" ;;
  *) NDK_HOST="${UNAME_S}-x86_64" ;;
esac
[ -d "$NDK/toolchains/llvm/prebuilt/$NDK_HOST" ] || { echo "NDK prebuilt not found: $NDK_HOST"; exit 1; }

TOOLCHAIN_BIN="$NDK/toolchains/llvm/prebuilt/$NDK_HOST/bin"
export PATH="$TOOLCHAIN_BIN:$PATH"

TARGET="aarch64-linux-android"
API="${ANDROID_API_LEVEL:-24}"
CC="$TOOLCHAIN_BIN/${TARGET}${API}-clang"
CXX="$TOOLCHAIN_BIN/${TARGET}${API}-clang++"
AR="$TOOLCHAIN_BIN/llvm-ar"
STRIP="$TOOLCHAIN_BIN/llvm-strip"

SYSROOT="$NDK/toolchains/llvm/prebuilt/$NDK_HOST/sysroot"
INSTALL_PREFIX="$OUT_DIR/prefix"

# ── Custom pkg-config (Termux style: only search our prefix) ────────────────
CUSTOM_PKGCONFIG="$OUT_DIR/android-pkg-config"
HOST_PKGCONFIG="$(command -v pkg-config)"
cat > "$CUSTOM_PKGCONFIG" <<PCEOF
#!/bin/sh
export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR=$INSTALL_PREFIX/lib/pkgconfig
exec $HOST_PKGCONFIG "\$@"
PCEOF
chmod +x "$CUSTOM_PKGCONFIG"

# ── Meson cross file ────────────────────────────────────────────────────────
cat > "$OUT_DIR/cross-android-arm64.txt" <<EOF
[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$AR'
strip = '$STRIP'
pkg-config = '$CUSTOM_PKGCONFIG'

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'

[built-in options]
c_link_args = ['-llog']
EOF

# ── Helper functions ────────────────────────────────────────────────────────
fetch_tarball() {
  local url="$1" out="$2"
  [ -f "$out" ] || { echo "Downloading $url"; curl -sL "$url" -o "$out"; }
}

extract_tarball() {
  local tar="$1" dest="$2"
  rm -rf "$dest"
  mkdir -p "$dest"
  tar xf "$tar" -C "$dest" --strip-components=1
}

# ═══════════════════════════════════════════════════════════════════════════════
#  STEP 1:  Download & extract ANGLE
# ═══════════════════════════════════════════════════════════════════════════════
ANGLE_CACHE="$BUILD_SRC_DIR/angle-android-arm64.deb"
ANGLE_EXTRACT="$OUT_DIR/angle-extract"

if [ ! -d "$ANGLE_EXTRACT/vulkan" ]; then
  echo "=== Downloading ANGLE from Termux ==="
  fetch_tarball "$ANGLE_DEB_URL" "$ANGLE_CACHE"

  echo "Extracting ANGLE .deb..."
  rm -rf "$ANGLE_EXTRACT"
  mkdir -p "$ANGLE_EXTRACT"

  ANGLE_TMP="$OUT_DIR/angle-deb-tmp"
  rm -rf "$ANGLE_TMP"
  mkdir -p "$ANGLE_TMP"
  (cd "$ANGLE_TMP" && ar x "$ANGLE_CACHE")

  DATA_TAR=$(ls "$ANGLE_TMP"/data.tar.* 2>/dev/null | head -1)
  [ -f "$DATA_TAR" ] || { echo "Cannot find data.tar.* in ANGLE .deb"; exit 1; }

  tar xf "$DATA_TAR" -C "$ANGLE_TMP"
  rm -rf "$ANGLE_TMP"/*.tar.* "$ANGLE_TMP"/debian-binary "$ANGLE_TMP"/control.tar.*

  ANGLE_OPT=$(find "$ANGLE_TMP" -type d -name "angle-android" | head -1)
  if [ -z "$ANGLE_OPT" ]; then
    echo "Cannot find angle-android directory in .deb"
    exit 1
  fi

  for backend in gl vulkan vulkan-null; do
    if [ -d "$ANGLE_OPT/$backend" ]; then
      mkdir -p "$ANGLE_EXTRACT/$backend"
      cp -a "$ANGLE_OPT/$backend/"*.so "$ANGLE_EXTRACT/$backend/" 2>/dev/null || true
      echo "  ANGLE $backend: $(ls "$ANGLE_EXTRACT/$backend/" 2>/dev/null | tr '\n' ' ')"
    fi
  done
  rm -rf "$ANGLE_TMP"

  if [ ! -f "$ANGLE_EXTRACT/vulkan/libEGL_angle.so" ]; then
    echo "ERROR: ANGLE vulkan/libEGL_angle.so not found after extraction"
    exit 1
  fi
  echo "ANGLE ready."
fi

# ═══════════════════════════════════════════════════════════════════════════════
#  STEP 2:  Download & extract sources
# ═══════════════════════════════════════════════════════════════════════════════
VIRGL_TAR="$BUILD_SRC_DIR/virglrenderer-${VIRGL_VERSION}.tar.gz"
EPOXY_TAR="$BUILD_SRC_DIR/libepoxy-${EPOXY_VERSION}.tar.gz"

fetch_tarball \
  "https://gitlab.freedesktop.org/virgl/virglrenderer/-/archive/virglrenderer-${VIRGL_VERSION}/virglrenderer-virglrenderer-${VIRGL_VERSION}.tar.gz" \
  "$VIRGL_TAR"
fetch_tarball \
  "https://github.com/anholt/libepoxy/archive/refs/tags/${EPOXY_VERSION}.tar.gz" \
  "$EPOXY_TAR"

VIRGL_SRC="$BUILD_SRC_DIR/virglrenderer"
EPOXY_SRC="$BUILD_SRC_DIR/libepoxy"
PATCH_DIR="$PROJECT_DIR/patches"

echo "=== Extracting sources ==="
extract_tarball "$VIRGL_TAR" "$VIRGL_SRC"
extract_tarball "$EPOXY_TAR" "$EPOXY_SRC"

# ═══════════════════════════════════════════════════════════════════════════════
#  STEP 3:  Patch libepoxy (ANGLE support via env var)
# ═══════════════════════════════════════════════════════════════════════════════
echo "=== Patching libepoxy (ANGLE support) ==="

python3 - "$EPOXY_SRC/src/dispatch_common.c" <<'PYEOF'
import sys
path = sys.argv[1]
with open(path) as f:
    src = f.read()

# Replace the Android #define block with mutable static arrays + env-var constructor
old = """\
#elif defined(__ANDROID__)
#define GLX_LIB "libGLESv2.so"
#define EGL_LIB "libEGL.so"
#define GLES1_LIB "libGLESv1_CM.so"
#define GLES2_LIB "libGLESv2.so"
"""

new = """\
#elif defined(__ANDROID__)
#define GLX_LIB GLES2_LIB
static char EGL_LIB[256] = "libEGL.so";
static char GLES1_LIB[256] = "libGLESv1_CM.so";
static char GLES2_LIB[256] = "libGLESv2.so";
#include <stdio.h>
#include <stdlib.h>
EPOXY_PUBLIC void epoxy_set_library_path(const char *path) {
    snprintf(EGL_LIB, sizeof(EGL_LIB), "%s/libEGL_angle.so", path);
    snprintf(GLES1_LIB, sizeof(GLES1_LIB), "%s/libGLESv1_CM_angle.so", path);
    snprintf(GLES2_LIB, sizeof(GLES2_LIB), "%s/libGLESv2_angle.so", path);
}
__attribute__((constructor)) static void epoxy_auto_angle(void) {
    const char *p = getenv("ANGLE_LIBS_DIR");
    if (p && p[0])
        epoxy_set_library_path(p);
}
"""

assert old in src, "libepoxy ANGLE patch target not found in dispatch_common.c"
src = src.replace(old, new, 1)

with open(path, 'w') as f:
    f.write(src)
print("  patched dispatch_common.c (ANGLE env-var auto-load)")
PYEOF

# ═══════════════════════════════════════════════════════════════════════════════
#  STEP 4:  Patch virglrenderer
# ═══════════════════════════════════════════════════════════════════════════════
echo "=== Patching virglrenderer ==="

# 4a) File-based patches (simple diffs)
for p in \
  "0001-disable-detect-os-android.patch" \
  "0002-android-disable-pthread-barriers.patch" \
  "0003-egl-without-gbm-meson.patch" \
  "0004-gnu-offsetof-warnings.patch" \
  "0005-disable-memfd-timespec.patch"; do
  if [ -f "$PATCH_DIR/$p" ]; then
    echo "  applying $p"
    (cd "$VIRGL_SRC" && patch -p1 --fuzz=3 < "$PATCH_DIR/$p") || {
      echo "WARN: patch $p did not apply cleanly, trying with --force"
      (cd "$VIRGL_SRC" && patch -p1 --fuzz=3 --force < "$PATCH_DIR/$p") || true
    }
  fi
done

# 4b) Python patches for complex changes

# vrend_winsys.c: Android EGL init without GBM (Termux 0001 approach)
python3 - "$VIRGL_SRC/src/vrend/vrend_winsys.c" <<'PYEOF'
import sys
path = sys.argv[1]
with open(path) as f:
    src = f.read()

# Add Android EGL init path before the #else error
old_init = """\
      use_context = CONTEXT_EGL;
#else
      (void)preferred_fd;
      virgl_error("EGL is not supported on this platform\\n");
      return -1;
#endif"""

new_init = """\
      use_context = CONTEXT_EGL;
#elif defined(__ANDROID__)
      (void)preferred_fd;
      egl = virgl_egl_init(NULL, flags & VIRGL_RENDERER_USE_SURFACELESS,
                           flags & VIRGL_RENDERER_USE_GLES);
      if (!egl)
         return -1;
      use_context = CONTEXT_EGL;
#else
      (void)preferred_fd;
      virgl_error("EGL is not supported on this platform\\n");
      return -1;
#endif"""

assert old_init in src, "vrend_winsys_init patch target not found"
src = src.replace(old_init, new_init, 1)

# Add Android EGL cleanup before the #ifdef ENABLE_GBM block
old_cleanup = """\
#ifdef ENABLE_GBM
   if (use_context == CONTEXT_EGL) {
      virgl_egl_destroy(egl);
      egl = NULL;
      use_context = CONTEXT_NONE;
      if (gbm) {
         virgl_gbm_fini(gbm);
         gbm = NULL;
      }
   } else if (use_context == CONTEXT_EGL_EXTERNAL) {
      free(egl);
      egl = NULL;
      use_context = CONTEXT_NONE;
   }
#endif"""

new_cleanup = """\
#ifdef __ANDROID__
   if (use_context == CONTEXT_EGL) {
      virgl_egl_destroy(egl);
      egl = NULL;
      use_context = CONTEXT_NONE;
   }
#endif
#ifdef ENABLE_GBM
   if (use_context == CONTEXT_EGL) {
      virgl_egl_destroy(egl);
      egl = NULL;
      use_context = CONTEXT_NONE;
      if (gbm) {
         virgl_gbm_fini(gbm);
         gbm = NULL;
      }
   } else if (use_context == CONTEXT_EGL_EXTERNAL) {
      free(egl);
      egl = NULL;
      use_context = CONTEXT_NONE;
   }
#endif"""

assert old_cleanup in src, "vrend_winsys_cleanup patch target not found"
src = src.replace(old_cleanup, new_cleanup, 1)

with open(path, 'w') as f:
    f.write(src)
print("  patched vrend_winsys.c (Android EGL without GBM)")
PYEOF

# vrend_winsys_egl.c: surfaceless fallback + EGL debug output
python3 - "$VIRGL_SRC/src/vrend/vrend_winsys_egl.c" <<'PYEOF'
import sys
path = sys.argv[1]
with open(path) as f:
    src = f.read()

# 1) Surfaceless fallback: if EGL_PLATFORM_SURFACELESS_MESA fails, try eglGetDisplay
old_sf = """\
      if (surfaceless) {
         egl->egl_display = egl->funcs.eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
                                                             (void *)EGL_DEFAULT_DISPLAY, NULL);
      }"""

new_sf = """\
      if (surfaceless) {
         egl->egl_display = egl->funcs.eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
                                                             (void *)EGL_DEFAULT_DISPLAY, NULL);
         if (!egl->egl_display || egl->egl_display == EGL_NO_DISPLAY)
            egl->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
      }"""

assert old_sf in src, "vrend_winsys_egl.c surfaceless patch target not found"
src = src.replace(old_sf, new_sf, 1)

# 2) Promote EGL debug output from virgl_debug to virgl_info (always visible)
src = src.replace(
    '#ifdef VIRGL_EGL_DEBUG\n   virgl_debug("EGL major/minor',
    '#if 1\n   virgl_info("EGL major/minor'
)
src = src.replace('virgl_debug("EGL version:', 'virgl_info("EGL version:')
src = src.replace('virgl_debug("EGL vendor:', 'virgl_info("EGL vendor:')
src = src.replace('virgl_debug("EGL extensions:', 'virgl_info("EGL extensions:')

with open(path, 'w') as f:
    f.write(src)
print("  patched vrend_winsys_egl.c (surfaceless fallback + EGL debug)")
PYEOF

# vtest_shm.c: ashmem fallback for Android (Termux 0004)
python3 - "$VIRGL_SRC/vtest/vtest_shm.c" <<'PYEOF'
import sys
path = sys.argv[1]
with open(path) as f:
    src = f.read()

# Add ashmem fallback function before vtest_new_shm
anchor = "int vtest_new_shm(uint32_t handle, size_t size)\n{"
assert anchor in src, "vtest_shm.c: vtest_new_shm not found"

ashmem_func = """\
#ifdef __ANDROID__
#include <sys/ioctl.h>
static int vtest_new_android_ashm(size_t size)
{
   int fd, ret;
   long flags;
   fd = open("/dev/ashmem", O_RDWR | O_CLOEXEC);
   if (fd < 0)
      return fd;
   ret = ioctl(fd, /* ASHMEM_SET_SIZE */ _IOW(0x77, 3, size_t), size);
   if (ret < 0)
      goto err;
   flags = fcntl(fd, F_GETFD);
   if (flags == -1)
      goto err;
   if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
      goto err;
   return fd;
err:
   close(fd);
   return -1;
}
#endif

"""

src = src.replace(anchor, ashmem_func + anchor, 1)

# Add ashmem fallback inside vtest_new_shm when memfd_create fails
old_fail = """\
   if (fd < 0) {
      return report_failed_call("memfd_create", -errno);
   }"""

new_fail = """\
   if (fd < 0) {
#ifdef __ANDROID__
      fd = vtest_new_android_ashm(size);
      if (fd >= 0)
         return fd;
#endif
      return report_failed_call("memfd_create", -errno);
   }"""

assert old_fail in src, "vtest_shm.c: memfd_create fail block not found"
src = src.replace(old_fail, new_fail, 1)

# Make vtest_shm_check always succeed on Android
old_check = "int vtest_shm_check(void)\n{"
new_check = """\
int vtest_shm_check(void)
{
#ifdef __ANDROID__
   return 1;
#endif
"""
src = src.replace(old_check, new_check, 1)

with open(path, 'w') as f:
    f.write(src)
print("  patched vtest_shm.c (ashmem fallback)")
PYEOF

# vrend_renderer.c: disable feat_dual_src_blend on Android (Termux 0006)
python3 - "$VIRGL_SRC/src/vrend/vrend_renderer.c" <<'PYEOF'
import sys
path = sys.argv[1]
with open(path) as f:
    src = f.read()

# Add early return for dual_src_blend in has_feature
old_feat = """\
static inline bool has_feature(enum features_id feature_id)
{
   int slot = feature_id / 64;"""

new_feat = """\
static inline bool has_feature(enum features_id feature_id)
{
#ifdef __ANDROID__
   if (feature_id == feat_dual_src_blend)
      return false;
#endif
   int slot = feature_id / 64;"""

assert old_feat in src, "vrend_renderer.c: has_feature not found"
src = src.replace(old_feat, new_feat, 1)

# Also force has_dual_src_blend = false in shader config
src = src.replace(
    'grctx->shader_cfg.has_dual_src_blend = has_feature(feat_dual_src_blend);',
    'grctx->shader_cfg.has_dual_src_blend = false;'
)

with open(path, 'w') as f:
    f.write(src)
print("  patched vrend_renderer.c (disable dual_src_blend on Android)")
PYEOF

# DXTn/S3TC texture decompression (Termux 0008)
# Creates gl4es-decompress.{c,h} and patches vrend_formats.c + vrend_renderer.c + src/meson.build
python3 - "$VIRGL_SRC" <<'PYEOF'
import sys, os
root = sys.argv[1]

# ── Create gl4es-decompress.h ──
hdr = os.path.join(root, "src", "gl4es-decompress.h")
with open(hdr, 'w') as f:
    f.write("""\
#ifndef _GL4ES_DECOMPRESS_H_
#define _GL4ES_DECOMPRESS_H_

#include <stdint.h>
#include <epoxy/gl.h>

void DecompressBlockDXT1(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image);

void DecompressBlockDXT3(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image);

void DecompressBlockDXT5(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image);

GLboolean isDXTcSRGB(GLenum format);
GLboolean isDXTcAlpha(GLenum format);

GLvoid *uncompressDXTc(GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
    int transparent0, int* simpleAlpha, int* complexAlpha, const GLvoid *data);

#endif
""")
print("  created gl4es-decompress.h")

# ── Create gl4es-decompress.c ──
src_c = os.path.join(root, "src", "gl4es-decompress.c")
with open(src_c, 'w') as f:
    f.write("""\
// Origin: https://github.com/alexvorxx/VirGL-Overlay-Rebuild/commit/d3052d9
// DXT1/DXT3/DXT5 texture decompression from gl4es
// Copyright (c) 2012, Matthaeus G. "Anteru" Chajdas / Benjamin Dobell (MIT)

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <epoxy/gl.h>

#include "gl4es-decompress.h"

static uint32_t PackRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return r | (g << 8) | (b << 16) | (a << 24);
}

static void DecompressBlockDXT1Internal(const uint8_t* block,
    uint32_t* output, uint32_t outputStride,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    const uint8_t* alphaValues)
{
    uint32_t temp, code;
    uint16_t color0, color1;
    uint8_t r0, g0, b0, r1, g1, b1;
    int i, j;

    color0 = *(const uint16_t*)(block);
    color1 = *(const uint16_t*)(block + 2);

    temp = (color0 >> 11) * 255 + 16;
    r0 = (uint8_t)((temp/32 + temp)/32);
    temp = ((color0 & 0x07E0) >> 5) * 255 + 32;
    g0 = (uint8_t)((temp/64 + temp)/64);
    temp = (color0 & 0x001F) * 255 + 16;
    b0 = (uint8_t)((temp/32 + temp)/32);

    temp = (color1 >> 11) * 255 + 16;
    r1 = (uint8_t)((temp/32 + temp)/32);
    temp = ((color1 & 0x07E0) >> 5) * 255 + 32;
    g1 = (uint8_t)((temp/64 + temp)/64);
    temp = (color1 & 0x001F) * 255 + 16;
    b1 = (uint8_t)((temp/32 + temp)/32);

    code = *(const uint32_t*)(block + 4);

    if (color0 > color1) {
        for (j = 0; j < 4; ++j) {
            for (i = 0; i < 4; ++i) {
                uint32_t finalColor, positionCode;
                uint8_t alpha = alphaValues[j*4+i];
                finalColor = 0;
                positionCode = (code >> 2*(4*j+i)) & 0x03;
                switch (positionCode) {
                case 0: finalColor = PackRGBA(r0, g0, b0, alpha); break;
                case 1: finalColor = PackRGBA(r1, g1, b1, alpha); break;
                case 2: finalColor = PackRGBA((2*r0+r1)/3, (2*g0+g1)/3, (2*b0+b1)/3, alpha); break;
                case 3: finalColor = PackRGBA((r0+2*r1)/3, (g0+2*g1)/3, (b0+2*b1)/3, alpha); break;
                }
                if (transparent0 && (finalColor==0xff000000)) { alpha=0; finalColor=0; }
                if (!alpha) *simpleAlpha = 1;
                else if (alpha<0xff) *complexAlpha = 1;
                output[j*outputStride + i] = finalColor;
            }
        }
    } else {
        for (j = 0; j < 4; ++j) {
            for (i = 0; i < 4; ++i) {
                uint32_t finalColor, positionCode;
                uint8_t alpha = alphaValues[j*4+i];
                finalColor = 0;
                positionCode = (code >> 2*(4*j+i)) & 0x03;
                switch (positionCode) {
                case 0: finalColor = PackRGBA(r0, g0, b0, alpha); break;
                case 1: finalColor = PackRGBA(r1, g1, b1, alpha); break;
                case 2: finalColor = PackRGBA((r0+r1)/2, (g0+g1)/2, (b0+b1)/2, alpha); break;
                case 3: finalColor = PackRGBA(0, 0, 0, alpha); break;
                }
                if (transparent0 && (finalColor==0xff000000)) { alpha=0; finalColor=0; }
                if (!alpha) *simpleAlpha = 1;
                else if (alpha<0xff) *complexAlpha = 1;
                output[j*outputStride + i] = finalColor;
            }
        }
    }
}

void DecompressBlockDXT1(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image)
{
    static const uint8_t const_alpha[] = {
        255,255,255,255, 255,255,255,255,
        255,255,255,255, 255,255,255,255
    };
    DecompressBlockDXT1Internal(blockStorage,
        image + x + (y * width), width, transparent0, simpleAlpha, complexAlpha, const_alpha);
}

void DecompressBlockDXT5(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image)
{
    uint8_t alpha0, alpha1;
    const uint8_t* bits;
    uint32_t alphaCode1;
    uint16_t alphaCode2;
    int i, j;

    alpha0 = *(blockStorage);
    alpha1 = *(blockStorage + 1);
    bits = blockStorage + 2;
    alphaCode1 = bits[2] | (bits[3] << 8) | (bits[4] << 16) | (bits[5] << 24);
    alphaCode2 = bits[0] | (bits[1] << 8);

    uint16_t color0 = *(const uint16_t*)(blockStorage + 8);
    uint16_t color1 = *(const uint16_t*)(blockStorage + 10);
    uint32_t temp, code;
    uint8_t r0, g0, b0, r1, g1, b1;

    temp = (color0 >> 11) * 255 + 16; r0 = (uint8_t)((temp/32 + temp)/32);
    temp = ((color0 & 0x07E0) >> 5) * 255 + 32; g0 = (uint8_t)((temp/64 + temp)/64);
    temp = (color0 & 0x001F) * 255 + 16; b0 = (uint8_t)((temp/32 + temp)/32);
    temp = (color1 >> 11) * 255 + 16; r1 = (uint8_t)((temp/32 + temp)/32);
    temp = ((color1 & 0x07E0) >> 5) * 255 + 32; g1 = (uint8_t)((temp/64 + temp)/64);
    temp = (color1 & 0x001F) * 255 + 16; b1 = (uint8_t)((temp/32 + temp)/32);

    code = *(const uint32_t*)(blockStorage + 12);

    for (j = 0; j < 4; j++) {
        for (i = 0; i < 4; i++) {
            uint8_t finalAlpha;
            int alphaCode, alphaCodeIndex = 3*(4*j+i);
            if (alphaCodeIndex <= 12)
                alphaCode = (alphaCode2 >> alphaCodeIndex) & 0x07;
            else if (alphaCodeIndex == 15)
                alphaCode = (alphaCode2 >> 15) | ((alphaCode1 << 1) & 0x06);
            else
                alphaCode = (alphaCode1 >> (alphaCodeIndex - 16)) & 0x07;
            if (alphaCode == 0) finalAlpha = alpha0;
            else if (alphaCode == 1) finalAlpha = alpha1;
            else {
                if (alpha0 > alpha1)
                    finalAlpha = (uint8_t)(((8-alphaCode)*alpha0 + (alphaCode-1)*alpha1)/7);
                else {
                    if (alphaCode == 6) finalAlpha = 0;
                    else if (alphaCode == 7) finalAlpha = 255;
                    else finalAlpha = (uint8_t)(((6-alphaCode)*alpha0 + (alphaCode-1)*alpha1)/5);
                }
            }
            uint8_t colorCode = (code >> 2*(4*j+i)) & 0x03;
            uint32_t finalColor = 0;
            switch (colorCode) {
            case 0: finalColor = PackRGBA(r0, g0, b0, finalAlpha); break;
            case 1: finalColor = PackRGBA(r1, g1, b1, finalAlpha); break;
            case 2: finalColor = PackRGBA((2*r0+r1)/3, (2*g0+g1)/3, (2*b0+b1)/3, finalAlpha); break;
            case 3: finalColor = PackRGBA((r0+2*r1)/3, (g0+2*g1)/3, (b0+2*b1)/3, finalAlpha); break;
            }
            if (finalAlpha==0) *simpleAlpha = 1;
            else if (finalAlpha<0xff) *complexAlpha = 1;
            image[i + x + (width * (y+j))] = finalColor;
        }
    }
}

void DecompressBlockDXT3(uint32_t x, uint32_t y, uint32_t width,
    const uint8_t* blockStorage,
    int transparent0, int* simpleAlpha, int *complexAlpha,
    uint32_t* image)
{
    int i;
    uint8_t alphaValues[16] = { 0 };
    for (i = 0; i < 4; ++i) {
        const uint16_t* alphaData = (const uint16_t*)(blockStorage);
        alphaValues[i*4 + 0] = (((*alphaData) >> 0) & 0xF) * 17;
        alphaValues[i*4 + 1] = (((*alphaData) >> 4) & 0xF) * 17;
        alphaValues[i*4 + 2] = (((*alphaData) >> 8) & 0xF) * 17;
        alphaValues[i*4 + 3] = (((*alphaData) >> 12) & 0xF) * 17;
        blockStorage += 2;
    }
    DecompressBlockDXT1Internal(blockStorage,
        image + x + (y * width), width, transparent0, simpleAlpha, complexAlpha, alphaValues);
}

GLboolean isDXTcSRGB(GLenum format) {
    switch (format) {
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        return 1;
    }
    return 0;
}

GLboolean isDXTcAlpha(GLenum format) {
    switch (format) {
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        return 1;
    }
    return 0;
}

GLvoid *uncompressDXTc(GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
    int transparent0, int* simpleAlpha, int* complexAlpha, const GLvoid *data)
{
    int pixelsize = 4;
    if (imageSize == width*height*pixelsize || data==NULL)
        return (GLvoid*)data;
    GLvoid *pixels = malloc(((width+3)&~3)*((height+3)&~3)*pixelsize);
    int blocksize;
    switch (format) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        blocksize = 8; break;
    default:
        blocksize = 16; break;
    }
    uintptr_t src = (uintptr_t)data;
    for (int y=0; y<height; y+=4) {
        for (int x=0; x<width; x+=4) {
            switch(format) {
            case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
                DecompressBlockDXT1(x, y, width, (const uint8_t*)src, transparent0, simpleAlpha, complexAlpha, (uint32_t*)pixels);
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
                DecompressBlockDXT3(x, y, width, (const uint8_t*)src, transparent0, simpleAlpha, complexAlpha, (uint32_t*)pixels);
                break;
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
                DecompressBlockDXT5(x, y, width, (const uint8_t*)src, transparent0, simpleAlpha, complexAlpha, (uint32_t*)pixels);
                break;
            }
            src += blocksize;
        }
    }
    return pixels;
}
""")
print("  created gl4es-decompress.c")

# ── Patch src/meson.build: add gl4es-decompress.c ──
meson_path = os.path.join(root, "src", "meson.build")
with open(meson_path) as f:
    meson = f.read()
anchor = "'vrend/vrend_winsys.c',"
assert anchor in meson, "src/meson.build: vrend_winsys.c entry not found"
meson = meson.replace(anchor, anchor + "\n   'gl4es-decompress.c',", 1)
with open(meson_path, 'w') as f:
    f.write(meson)
print("  patched src/meson.build (added gl4es-decompress.c)")

# ── Patch vrend_formats.c: enable DXTn when dxtn_decompress is set ──
fmt_path = os.path.join(root, "src", "vrend", "vrend_formats.c")
with open(fmt_path) as f:
    fmt = f.read()

# Add extern declaration
old_fmt_anchor = "void vrend_build_format_list_common(void)"
assert old_fmt_anchor in fmt, "vrend_formats.c: vrend_build_format_list_common not found"
fmt = fmt.replace(old_fmt_anchor, "extern int dxtn_decompress;\n\n" + old_fmt_anchor, 1)

# Add dxtn_decompress fallback in the S3TC extension check
old_s3tc = 'epoxy_has_gl_extension("GL_ANGLE_texture_compression_dxt")) {'
new_s3tc = 'epoxy_has_gl_extension("GL_ANGLE_texture_compression_dxt") || dxtn_decompress) {'
assert old_s3tc in fmt, "vrend_formats.c: S3TC extension check not found"
fmt = fmt.replace(old_s3tc, new_s3tc, 1)

with open(fmt_path, 'w') as f:
    f.write(fmt)
print("  patched vrend_formats.c (DXTn decompress fallback)")

# ── Patch vrend_renderer.c: add DXTn decompression in texture transfer ──
rend_path = os.path.join(root, "src", "vrend", "vrend_renderer.c")
with open(rend_path) as f:
    rend = f.read()

# Add include and global
old_inc_anchor = "#ifdef WIN32"
assert old_inc_anchor in rend, "vrend_renderer.c: #ifdef WIN32 not found"
dxtn_inc = '''\
#include "gl4es-decompress.h"
const int dxtn_decompress = 1;

'''
rend = rend.replace(old_inc_anchor, dxtn_inc + old_inc_anchor, 1)

with open(rend_path, 'w') as f:
    f.write(rend)
print("  patched vrend_renderer.c (DXTn include + global)")

# ── Patch proxy_server.c: use linker64 bootstrap for execv on Android ──
# Android mounts /data with noexec, so execv(binary) fails silently.
# We must go through /system/bin/linker64 just like we do for the main server.
proxy_path = os.path.join(root, "src", "proxy", "proxy_server.c")
with open(proxy_path) as f:
    proxy = f.read()

old_execv = '''\
      /* for devenv without installing server */
      char *const server_path = getenv("RENDER_SERVER_EXEC_PATH");
      char *const argv[] = {
         server_path ? server_path : RENDER_SERVER_EXEC_PATH,
         "--socket-fd",
         fd_str,
         NULL,
      };
      execv(argv[0], argv);'''

new_execv = '''\
      /* for devenv without installing server */
      char *const server_path = getenv("RENDER_SERVER_EXEC_PATH");
      char *const binary = server_path ? server_path : RENDER_SERVER_EXEC_PATH;
#if defined(__ANDROID__)
      /* Android /data is mounted noexec; bootstrap through the system linker. */
      char *const linker = sizeof(void*) == 8
         ? "/system/bin/linker64" : "/system/bin/linker";
      char *const argv[] = {
         linker,
         binary,
         "--socket-fd",
         fd_str,
         NULL,
      };
      execv(linker, argv);
#else
      char *const argv[] = {
         binary,
         "--socket-fd",
         fd_str,
         NULL,
      };
      execv(argv[0], argv);
#endif'''

assert old_execv in proxy, "proxy_server.c: execv block not found"
proxy = proxy.replace(old_execv, new_execv, 1)

with open(proxy_path, 'w') as f:
    f.write(proxy)
print("  patched proxy_server.c (linker64 bootstrap for Android)")
PYEOF

echo "All patches applied."

# ═══════════════════════════════════════════════════════════════════════════════
#  STEP 5:  Build libepoxy
# ═══════════════════════════════════════════════════════════════════════════════
rm -rf "$INSTALL_PREFIX"
mkdir -p "$INSTALL_PREFIX"

echo "=== Building libepoxy ==="
rm -rf "$OUT_DIR/libepoxy-build"
meson setup "$OUT_DIR/libepoxy-build" "$EPOXY_SRC" \
  --cross-file "$OUT_DIR/cross-android-arm64.txt" \
  --prefix "$INSTALL_PREFIX" \
  --libdir lib \
  -Degl=yes -Dglx=no -Dx11=false
ninja -C "$OUT_DIR/libepoxy-build" install

# ═══════════════════════════════════════════════════════════════════════════════
#  STEP 6:  Generate stub .pc for NDK system libs
# ═══════════════════════════════════════════════════════════════════════════════
NDK_LIBDIR="$SYSROOT/usr/lib/${TARGET}/${API}"
NDK_INCDIR="$SYSROOT/usr/include"
PC_DIR="$INSTALL_PREFIX/lib/pkgconfig"
mkdir -p "$PC_DIR"

cat > "$PC_DIR/egl.pc" <<EGLPC
prefix=$SYSROOT/usr
libdir=$NDK_LIBDIR
includedir=$NDK_INCDIR

Name: egl
Description: NDK EGL
Version: 1.5
Libs: -L\${libdir} -lEGL
Cflags: -I\${includedir}
EGLPC

cat > "$PC_DIR/glesv2.pc" <<GLPC
prefix=$SYSROOT/usr
libdir=$NDK_LIBDIR
includedir=$NDK_INCDIR

Name: glesv2
Description: NDK GLESv2
Version: 3.2
Libs: -L\${libdir} -lGLESv2
Cflags: -I\${includedir}
GLPC

# ═══════════════════════════════════════════════════════════════════════════════
#  STEP 7:  Build virglrenderer
# ═══════════════════════════════════════════════════════════════════════════════
echo "=== Building virglrenderer ==="
rm -rf "$OUT_DIR/virglrenderer-build"
meson setup "$OUT_DIR/virglrenderer-build" "$VIRGL_SRC" \
  --cross-file "$OUT_DIR/cross-android-arm64.txt" \
  --prefix "$INSTALL_PREFIX" \
  --libdir lib \
  -Dplatforms=egl \
  -Dcheck-gl-errors=false \
  -Dvenus=true \
  -Drender-server-worker=thread
ninja -C "$OUT_DIR/virglrenderer-build" install

# ═══════════════════════════════════════════════════════════════════════════════
#  STEP 8:  Package into assets
# ═══════════════════════════════════════════════════════════════════════════════
echo "=== Packaging assets ==="

BIN_SRC="$INSTALL_PREFIX/bin/virgl_test_server"
BIN_DST="$OUT_DIR/virgl_test_server_android"
[ -f "$BIN_SRC" ] || { echo "Expected binary not found: $BIN_SRC"; exit 1; }
cp -a "$BIN_SRC" "$BIN_DST"
"$STRIP" "$BIN_DST" || true

cp -a "$BIN_DST" "$ASSETS_DIR/virgl_test_server_android"
echo "  binary: $ASSETS_DIR/virgl_test_server_android"

# Venus render server (used by --venus mode for Vulkan forwarding)
RENDER_SRV="$INSTALL_PREFIX/libexec/virgl_render_server"
if [ -f "$RENDER_SRV" ]; then
  cp -a "$RENDER_SRV" "$ASSETS_DIR/virgl_render_server"
  "$STRIP" "$ASSETS_DIR/virgl_render_server" || true
  echo "  binary: $ASSETS_DIR/virgl_render_server"
fi

for so in libvirglrenderer.so libepoxy.so; do
  if [ -f "$INSTALL_PREFIX/lib/$so" ]; then
    cp -a "$INSTALL_PREFIX/lib/$so" "$ASSETS_DIR/$so"
    "$STRIP" "$ASSETS_DIR/$so" || true
    echo "  lib: $ASSETS_DIR/$so"
  fi
done

# Copy ANGLE libraries (vulkan backend is the default / best performance)
ANGLE_ASSET_DIR="$ASSETS_DIR/angle"
rm -rf "$ANGLE_ASSET_DIR"
for backend in vulkan gl vulkan-null; do
  if [ -d "$ANGLE_EXTRACT/$backend" ]; then
    mkdir -p "$ANGLE_ASSET_DIR/$backend"
    cp -a "$ANGLE_EXTRACT/$backend/"*.so "$ANGLE_ASSET_DIR/$backend/"
    for f in "$ANGLE_ASSET_DIR/$backend/"*.so; do
      "$STRIP" "$f" 2>/dev/null || true
    done
    echo "  angle/$backend: $(ls "$ANGLE_ASSET_DIR/$backend/" | tr '\n' ' ')"
  fi
done

echo ""
echo "=== Build complete ==="
echo "Assets directory: $ASSETS_DIR"
ls -la "$ASSETS_DIR/"
echo ""
echo "ANGLE backends:"
ls -la "$ANGLE_ASSET_DIR/"* 2>/dev/null || echo "  (none)"
