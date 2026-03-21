# Trierarch

[中文](README.zh.md) | English

---

This **monorepo** hosts the Android app and supporting code (`trierarch-app`, `trierarch-native`, `trierarch-proot`, `trierarch-wayland`, `trierarch-optimize`, and more). Primary home: **[github.com/Beauty114514/trierarch](https://github.com/Beauty114514/trierarch)**.

**Trierarch** is an Android app built on the **proot**-style container stack familiar from the **Termux** ecosystem—you **do not** need the Termux app installed. It runs an **Arch Linux** rootfs on your phone or tablet. Other desktop environments may be usable **in principle** (same proot + Wayland setup), but **development**, **documentation**, and **tuning** focus on **KDE Plasma** on **Wayland**. The app bundles its own **Wayland** compositor (pointer/touch, absolute or relative mode, soft keyboard → clients). Use the **Display startup script** in settings to launch your **Plasma (Wayland)** session. Jetpack Compose UI; JNI/native code lives in [`trierarch-native/`](trierarch-native/).

**Tuning & optimization:** documentation for improving apps and day-to-day use *inside* the desktop/rootfs starts at [`trierarch-optimize/README.md`](trierarch-optimize/README.md) ([中文](trierarch-optimize/README.zh.md)), which links to each tutorial (e.g. Firefox).

For developers who want to build from source, see [`README_DEV.md`](README_DEV.md).

**Contributing & security:** see [`CONTRIBUTING.md`](CONTRIBUTING.md) ([中文](CONTRIBUTING.zh.md)) and [`SECURITY.md`](SECURITY.md) ([中文](SECURITY.zh.md)). Changes are summarized in [`CHANGELOG.md`](CHANGELOG.md).

## Arch rootfs

- Rootfs path: `data_dir/arch` (app internal storage).
- **Auto-download**: If no rootfs exists at first launch, the app downloads the Arch Linux aarch64 rootfs from [proot-distro](https://github.com/termux/proot-distro/releases) (~156 MB) and extracts it. Requires network.
- **Manual**: You can put a Termux proot-distro Arch aarch64 rootfs there (e.g. download and extract manually).
- If `data_dir/arch` has no usable `sh`, proot runs with `-0 /system/bin/sh`.

## Wayland and Display

- Turn **Wayland** on in the side menu; switch to the Wayland view to see the compositor output. Touch acts as pointer (absolute or relative/touchpad mode in settings); use the Keyboard button to send key events to the focused client.
- **Display**: tap to run your **Display startup script** (configure it for **KDE Plasma** / Wayland); long-press to edit the script. The app skips re-running the script if a Wayland client is already connected.

## Input Tips (GTK / Qt)

We currently solved `Ctrl+Shift+U` for **GTK apps**, so you can enter Chinese, other non-ASCII characters, and Emoji more smoothly (for example in Firefox).

For **Qt apps**, `Ctrl+Shift+U` support may be incomplete due to underlying limitations. If you need Chinese / other non-ASCII input in a Qt app, the recommended workflow is:

- Enter the text in a **GTK app** that supports `Ctrl+Shift+U` first (e.g. Mousepad);
- Copy it, then paste into the Qt app (usually `Ctrl+V` / `Ctrl+Shift+V`, but the exact behavior depends on the target application).

If you prefer a more “full keyboard” experience on the soft keyboard, you may try [**Unexpected Keyboard**](https://play.google.com/store/apps/details?id=juloo.keyboard2) on [Google Play](https://play.google.com/store/apps/details?id=juloo.keyboard2) (open source: [GitHub](https://github.com/Julow/Unexpected-Keyboard)).

If we find a more universal solution in the future, we will update the project as soon as possible. Contributions and testing feedback from users and other open-source developers are also welcome.

## Acknowledgments

Trierarch stands on many open-source projects. Thanks to their authors and communities, including (non-exhaustively):

- **[PRoot](https://github.com/termux/proot)** and the **Termux** ecosystem; Arch rootfs from **[proot-distro](https://github.com/termux/proot-distro)** releases
- **[Wayland](https://wayland.freedesktop.org/)** and **wayland-protocols** (reference sources under `trierarch-wayland/`)
- **[libffi](https://github.com/libffi/libffi)** · **Rust** · **Kotlin** · **Jetpack Compose** · **Android** / **NDK**
- The **KDE Plasma** and **Arch Linux** communities (the stack we focus on in documentation)
- **[Unexpected Keyboard](https://github.com/Julow/Unexpected-Keyboard)** (optional third-party keyboard we mention in this README)

Bundled third-party code includes its own license files in-tree (e.g. `COPYING`, `LICENSE` under vendored paths). This section is a thank-you, not a complete legal notice.
