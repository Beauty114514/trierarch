# Trierarch (App)

[中文](README.zh.md) | English

---

Android app: run a Linux shell (optionally an Arch rootfs) via **proot** without Termux. Optional **Wayland** GUI: built-in compositor with pointer (touch, absolute/relative mode) and keyboard (soft keyboard → Wayland). Use the **Display startup script** (in app settings) to launch any desktop or window manager (e.g. sway, Xfce, KDE) inside proot. Jetpack Compose UI; native layer from [trierarch-native](https://github.com/Beauty114514/trierarch-native).

For developers who want to build from source, see [`README_DEV.md`](README_DEV.md).

## Arch rootfs

- Rootfs path: `data_dir/arch` (app internal storage).
- **Auto-download**: If no rootfs exists at first launch, the app downloads the Arch Linux aarch64 rootfs from [proot-distro](https://github.com/termux/proot-distro/releases) (~156 MB) and extracts it. Requires network.
- **Manual**: You can put a Termux proot-distro Arch aarch64 rootfs there (e.g. download and extract manually).
- If `data_dir/arch` has no usable `sh`, proot runs with `-0 /system/bin/sh`.

## Wayland and Display

- Turn **Wayland** on in the side menu; switch to the Wayland view to see the compositor output. Touch acts as pointer (absolute or relative/touchpad mode in settings); use the Keyboard button to send key events to the focused client.
- **Display**: tap to run your **Display startup script** (e.g. `sway` or `startx`); long-press to edit the script. The app skips re-running the script if a Wayland client is already connected.

## Input Tips (GTK / Qt)

We currently solved `Ctrl+Shift+U` for **GTK apps**, so you can enter Chinese, other non-ASCII characters, and Emoji more smoothly (for example in Firefox).

For **Qt apps**, `Ctrl+Shift+U` support may be incomplete due to underlying limitations. If you need Chinese / other non-ASCII input in a Qt app, the recommended workflow is:

- Enter the text in a **GTK app** that supports `Ctrl+Shift+U` first (e.g. Mousepad);
- Copy it, then paste into the Qt app (usually `Ctrl+V` / `Ctrl+Shift+V`, but the exact behavior depends on the target application).

If you prefer a more “full keyboard” experience on the soft keyboard, you may try **Unexpected Keyboard**.

If we find a more universal solution in the future, we will update the project as soon as possible. Contributions and testing feedback from users and other open-source developers are also welcome.
