#!/usr/bin/env bash
# Merge ../pulse-android into $PULSE_SRC. No Meson. FORCE_INTEGRATE=1 after git reset to re-patch.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="$ROOT/pulse-android"
if [[ -z "${PULSE_SRC:-}" && -f "$ROOT/third_party/pulseaudio/meson.build" ]]; then
  PULSE_SRC="$ROOT/third_party/pulseaudio"
fi
SRC="${PULSE_SRC:-}"
INTEGRATED="$SRC/.trierarch-pulse-integrated"

if [[ -z "$SRC" || ! -f "$SRC/meson.build" ]]; then
  echo "Set PULSE_SRC to the root of a pulseaudio checkout (e.g. v17.0), or run:"
  echo "  ./build-pulse-android.sh"
  exit 1
fi

if [[ ! -d "$PORT/patches" || ! -d "$PORT/modules" ]]; then
  echo "Missing $PORT/patches or $PORT/modules"
  exit 1
fi

if [[ -f "$INTEGRATED" && "${FORCE_INTEGRATE:-0}" != "1" ]]; then
  echo "already integrated: $SRC  (FORCE_INTEGRATE=1 + git clean)"
  exit 0
fi

if [[ "${FORCE_INTEGRATE:-0}" == "1" ]]; then
  if [[ -d "$SRC/.git" ]]; then
    git -C "$SRC" reset --hard HEAD
    git -C "$SRC" clean -fdx
  else
    echo "FORCE_INTEGRATE=1 needs a git clone; rm -rf the tree and run ./build-pulse-android.sh again."
    exit 1
  fi
  rm -f "$INTEGRATED"
fi

echo "PULSE_SRC=$SRC"
echo "PORT=$PORT"

# Match Termux pre_configure: drop Android sink sources into the tree.
mkdir -p "$SRC/src/modules/sles" "$SRC/src/modules/aaudio"
cp -f "$PORT/modules/module-sles-sink.c" "$PORT/modules/module-sles-source.c" \
  "$SRC/src/modules/sles/"
cp -f "$PORT/modules/module-aaudio-sink.c" "$SRC/src/modules/aaudio/"

# Apply vendored patches (order: priv drop → paths → meson registers modules → NLS).
for p in no_priv_drop.patch fix-paths.patch meson.patch disable-nls-without-dgettext.patch; do
  f="$PORT/patches/$p"
  if [[ ! -f "$f" ]]; then
    echo "missing patch: $f"
    exit 1
  fi
  echo "applying $p"
  patch -d "$SRC" -p1 < "$f"
done

touch "$INTEGRATED"

echo "integrate done."
