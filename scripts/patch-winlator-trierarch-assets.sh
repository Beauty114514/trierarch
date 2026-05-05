#!/usr/bin/env bash
# Repack Winlator rootfs / box64 / graphics tzst blobs so aarch64 ELFs use Trierarch on-disk paths.
# Requires: patchelf, tar with zstd support, file, find, bash.
#
# Usage (from repo root):
#   ./scripts/patch-winlator-trierarch-assets.sh
#
# On hosts without patchelf (BusyBox tar lacks --zstd; install GNU tar):
#   docker run --rm -v "$PWD:/work" -w /work alpine:3.19 sh -lc \
#     'apk add --no-cache bash tar zstd patchelf file findutils >/dev/null && ./scripts/patch-winlator-trierarch-assets.sh'
#
set -euo pipefail

OLD_ROOT=/data/data/com.winlator/files/rootfs
NEW_ROOT=/data/user/0/app.trierarch/files/wine
NEW_INTERP="${NEW_ROOT}/lib/ld-linux-aarch64.so.1"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ASSETS="${REPO_ROOT}/trierarch-app/app/src/main/assets"
JNILIBS="${REPO_ROOT}/trierarch-app/app/src/main/jniLibs/arm64-v8a"

BACKUP=1
DRY_RUN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-backup) BACKUP=0 ;;
    --dry-run) DRY_RUN=1 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
  shift
done

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing command: $1" >&2
    exit 1
  }
}

need_cmd patchelf
need_cmd tar
need_cmd file
need_cmd find
need_cmd python3

# Wineserver (and similar) embed OLD_ROOT in .rodata (e.g. tmp/.wine-%u, nls paths). Patchelf does not touch that.
# Replace with a same-length /data/data/app.trierarch/... prefix so Android resolves into this app's tree.
NEW_EMB_ROOT="/data/data/app.trierarch/files/wine/"
patch_embedded_old_root_strings() {
  local f="$1"
  file -b "$f" | grep -qi ELF || return 0
  [[ "${#OLD_ROOT}" -eq "${#NEW_EMB_ROOT}" ]] || {
    echo "BUG: OLD_ROOT and NEW_EMB_ROOT must be same length (${#OLD_ROOT} vs ${#NEW_EMB_ROOT})" >&2
    exit 1
  }
  PATCH_OLD="$OLD_ROOT" PATCH_NEW="$NEW_EMB_ROOT" PATCH_DRY="$DRY_RUN" python3 -c '
import os, sys
path = sys.argv[1]
old = os.environ["PATCH_OLD"].encode()
new = os.environ["PATCH_NEW"].encode()
dry = os.environ.get("PATCH_DRY", "0") != "0"
assert len(old) == len(new), (len(old), len(new))
with open(path, "rb") as fh:
    data = fh.read()
if old not in data:
    sys.exit(0)
if dry:
    print("  embedded-root (dry-run):", path)
    sys.exit(0)
n = data.count(old)
data = data.replace(old, new)
with open(path, "w+b") as fh:
    fh.write(data)
print("  embedded-root x%d: %s" % (n, path))
' "$f"
}

patch_one_elf() {
  local f="$1"
  file -b "$f" | grep -qi ELF || return 0

  local interp=""
  interp="$(patchelf --print-interpreter "$f" 2>/dev/null || true)"
  if [[ -n "$interp" ]]; then
    if [[ "$interp" == *com.winlator* ]] || [[ "$interp" == *"${OLD_ROOT}"* ]]; then
      echo "  set-interpreter: $f"
      if [[ "$DRY_RUN" -eq 0 ]]; then
        patchelf --set-interpreter "$NEW_INTERP" "$f"
      fi
    fi
  fi

  local rp=""
  rp="$(patchelf --print-rpath "$f" 2>/dev/null || true)"
  if [[ -n "$rp" ]] && [[ "$rp" == *com.winlator* ]]; then
    local newrp="${rp//$OLD_ROOT/$NEW_ROOT}"
    echo "  set-rpath: $f"
    if [[ "$DRY_RUN" -eq 0 ]]; then
      patchelf --set-rpath "$newrp" "$f"
    fi
  fi
}

patch_tree() {
  local root="$1"
  while IFS= read -r -d '' f; do
    patch_one_elf "$f"
    patch_embedded_old_root_strings "$f"
  done < <(find "$root" -type f -print0)
}

# winlator-app sets LD_LIBRARY_PATH to guest usr/lib only. Upstream tzsts often ship Mesa also as
# usr/lib/libEGL.so.1 -> aarch64-linux-gnu/...; if only triplet has the file, dlopen fails. Mirror that layout.
ensure_mesa_symlinks_in_usr_lib() {
  local root="$1"
  local triplet="${root}/usr/lib/aarch64-linux-gnu"
  [[ -d "$triplet" ]] || return 0
  mkdir -p "${root}/usr/lib"
  local lib
  for lib in libEGL.so.1 libGLESv2.so.2 libgbm.so.1 libGL.so.1; do
    if [[ -f "${triplet}/${lib}" ]] && [[ ! -e "${root}/usr/lib/${lib}" ]]; then
      if [[ "$DRY_RUN" -eq 0 ]]; then
        ln -sfn "aarch64-linux-gnu/${lib}" "${root}/usr/lib/${lib}"
      fi
      echo "  mesa-symlink usr/lib/${lib} -> aarch64-linux-gnu/${lib}"
    fi
  done
}

patch_tzst() {
  local tzst="$1"
  echo "==> $tzst"
  [[ -f "$tzst" ]] || { echo "  skip (missing)"; return 0; }

  if [[ "$BACKUP" -eq 1 ]] && [[ "$DRY_RUN" -eq 0 ]] && [[ ! -f "${tzst}.orig" ]]; then
    cp -a "$tzst" "${tzst}.orig"
  fi

  local work
  work="$(mktemp -d)"
  tar -I zstd -xf "$tzst" -C "$work"

  patch_tree "$work"
  ensure_mesa_symlinks_in_usr_lib "$work"

  if [[ "$DRY_RUN" -eq 0 ]]; then
    # Avoid "tar --zstd -19" (some tars parse -19 as invalid); use explicit compressor.
    # GNU tar may warn "file changed as we read it" when archiving a tree we just modified; safe to ignore.
    # Write the archive next to the final path (same filesystem). $work is often on tmpfs; mv from tmpfs
    # to app assets (bind mount) fails with EXDEV.
    local repack="${tzst}.repack.$$"
    ( cd "$work" && tar --warning=no-file-changed -I 'zstd -19' -cf "$repack" . )
    mv "$repack" "$tzst"
  fi
  rm -rf "$work"
}

patch_jni_so() {
  local so="$1"
  echo "==> $so"
  if [[ "$BACKUP" -eq 1 ]] && [[ "$DRY_RUN" -eq 0 ]] && [[ ! -f "${so}.orig" ]]; then
    cp -a "$so" "${so}.orig"
  fi
  if [[ "$DRY_RUN" -eq 0 ]]; then
    patch_one_elf "$so"
    patch_embedded_old_root_strings "$so"
  else
    patch_one_elf "$so"
    patch_embedded_old_root_strings "$so"
  fi
}

echo "OLD_ROOT=$OLD_ROOT"
echo "NEW_ROOT=$NEW_ROOT"
echo "ASSETS=$ASSETS"

shopt -s nullglob
TZSTS=(
  "${ASSETS}/rootfs.tzst"
  "${ASSETS}/rootfs_patches.tzst"
  "${ASSETS}/pulseaudio.tzst"
  "${ASSETS}/container_pattern.tzst"
  "${ASSETS}/box64/"*.tzst
  "${ASSETS}/graphics_driver/"*.tzst
)

for tzst in "${TZSTS[@]}"; do
  [[ -e "$tzst" ]] || continue
  patch_tzst "$tzst"
done

echo "==> jniLibs (PT_INTERP / RPATH + embedded OLD_ROOT bytes when present)"
if [[ -d "$JNILIBS" ]]; then
  while IFS= read -r -d '' so; do
    file -b "$so" | grep -qi ELF || continue
    patch_jni_so "$so"
  done < <(find "$JNILIBS" -type f -name '*.so' -print0)
fi

echo "Done."
