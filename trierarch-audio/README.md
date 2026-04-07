# trierarch-audio

[中文](README.zh.md) | English

**PulseAudio** integration for the host app, under the [Trierarch monorepo](https://github.com/Beauty114514/trierarch). On device: prefix **`filesDir/pulse/`** (`bin/pulseaudio`, `lib/`, …). Guest: **`libpulse`** connects over a **Unix socket** (`PULSE_SERVER=unix:/run/trierarch-pulse/native`). Host: **`trierarch-native`** `pulse_host` starts the daemon (AAudio sink).

## Repository layout (tracked vs generated)

- **Tracked (this repo)**: `build-android/`, `pulse-android/`, `README*.md`, `.gitignore`
- **Generated (ignored)**: `third_party/` (upstream clones), `out/` (sysroot + install prefix)

## Prerequisites

- **git** (clone upstream)
- For cross-build: **Android NDK**, **meson**, **ninja** (plus deps you enable)

## Manual build

**1.** Clone upstream and merge this repo’s port (patches target **v17.0**):

```bash
git clone --depth 1 --branch v17.0 https://github.com/pulseaudio/pulseaudio.git
export PULSE_SRC="$(pwd)/pulseaudio"
cd trierarch-audio/build-android
./build-pulse.sh
```

**2.** In **`$PULSE_SRC`**, **NDK + Meson** then **`meson install`** into a layout matching **`getFilesDir()/pulse/`** on device (flags: Termux **`packages/pulseaudio`**).

After changing **`pulse-android/`** patches: **`git reset --hard && git clean -fdx`** in the clone, then **`FORCE_INTEGRATE=1 ./build-pulse.sh`**, or remove **`third_party/pulseaudio`** and use the script path.

If silent: **`load-module`** for SLES/AAudio in **`default.pa`** or **`pulse_host.rs`**.

## Script build

Default clone dir is **`trierarch-audio/third_party/pulseaudio`**:

```bash
cd trierarch-audio/build-android
./build-pulse-android.sh
```

`build-pulse-android.sh` will build the sysroot deps, run the Meson cross build + install into **`trierarch-audio/out/pulse-android-prefix`**, then **sync the prefix into the app assets** under **`trierarch-app/app/src/main/assets/pulse/`** (disable with `PULSE_SKIP_INSTALL_TO_APP=1`).

## Using the artifacts

Install the **`meson install`** prefix (at least **`bin/pulseaudio`** and **`lib/`**) under **`getFilesDir()/pulse/`** (or other paths **`pulse_host`** checks). See **`pulse_host`** in **`trierarch-native`**.

| Path | Role |
|------|------|
| `pulse-android/` | patches, `modules/*.c` |
| `build-android/` | **`build-pulse-android.sh`** (clone + integrate), **`build-pulse.sh`** (integrate only; needs **`PULSE_SRC`**) |
