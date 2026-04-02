# Trierarch

[中文](README.zh.md) | English

---

**Download:** latest APK → **[GitHub Releases](https://github.com/Beauty114514/trierarch/releases/latest)**.

## Demo

| KDE Plasma (Wayland) | Firefox (inside proot) |
|------------------------|-------------------------|
| ![KDE Plasma desktop](docs/images/desktop.jpg) | ![Firefox](docs/images/firefox.jpg) |

---

**Trierarch** is an Android app that runs an **Arch Linux** rootfs using a **proot**-style container, with a custom **Wayland** compositor (development and tuning focus on **KDE Plasma (Wayland)**). The goal is your own **Arch** + **KDE Plasma 6 (Wayland)** mobile desktop.

## 1. First launch: automatic rootfs download

- If there is no usable rootfs under `data_dir/arch` at first launch, the app **downloads and extracts** the Arch Linux aarch64 rootfs from [proot-distro](https://github.com/termux/proot-distro/releases) (~156 MB). Network required.

## 2. Before the desktop: enter Terminal (shortcut) and install Plasma inside proot

Long-press the app icon to show **shortcuts**, then tap **Terminal** to enter the **Arch shell inside proot**.

![App shortcut](docs/images/shortcut.jpg)

In the proot Arch shell, **update first, then install the desktop** (example; adjust packages as you like):

```bash
pacman -Syu
pacman -S plasma-desktop dolphin konsole
```

## 3. Floating menu orb and Display (open menu, set Display script, start the desktop)

- **Floating menu orb** — A draggable **orb** on screen; **short tap** opens a **glass menu** (**Display**, **View Settings**, **Appearance**, **Keyboard**, and related actions). Drag the orb to move it; position is remembered between sessions.

![Floating menu orb](docs/images/floatingOrb.jpg)

| Menu open (terminal / Wayland view) | Menu open (desktop session) |
|--------------------------------------|-----------------------------|
| ![Orb menu in terminal view](docs/images/floutMenuInTerminal.jpg) | ![Orb menu on desktop](docs/images/floutMenuInDesktop.jpg) |

- **Display** — **Long-press** to edit the **Display startup script**; **short-press** to run it and start Plasma. **If a Wayland client is already connected, the app does not run the script again.**

**Recommended script** (session D-Bus via `dbus-launch`, then Plasma Wayland; redirect output so the script doesn’t block):

```bash
dbus-launch --exit-with-session startplasma-wayland > /dev/null 2>&1 &
```

![Display in orb menu](docs/images/displayScript.jpg)

## 4. Daily use: tap app to auto-start the desktop

After you have set a Display startup script, you can **tap the app icon normally** to enter the desktop view; the app will **inject and run the Display script automatically** (idempotent when a desktop client is already connected).

The **Terminal** shortcut is mainly for initialization (installing a desktop + terminal app) or as a fallback when you don’t have a terminal available inside the desktop yet.

## 5. After Plasma starts: View Settings and terminal ↔ desktop switching

### View Settings

Open **View Settings** from the orb menu to tune the compositor and pointer behavior:

![View Settings](docs/images/viewSettings.jpg)

- **Pointer / mouse mode** — e.g. **touchpad-style (relative)** vs **tablet-style (absolute)**.
- **UI size vs clarity** — **Resolution** and **Scale** can be **combined**: Resolution lowers output resolution to reduce compositing load **and** adjusts on-screen element size; Scale resizes via scaling without changing the backing resolution to keep text/UI sharp. There’s no single “correct” mix—tune until it fits **your** device and habits.

### Switching between terminal and desktop with Display

After Plasma is running, tap **Display** in the orb menu again to return to the **terminal / Wayland** view; **open the orb menu → Display** to go back to the desktop. The orb menu and Display work in both the shell view and on the desktop.

## 6. Keyboard and input (orb menu Keyboard, GTK / Qt)

The **Keyboard** item in the orb menu can **invoke** the soft keyboard.

![Keyboard in orb menu](docs/images/keyboard.jpg)

In **GTK** apps you can enter **non-ASCII** characters (Chinese, emoji, special characters, etc.); input is completed automatically via the **`Ctrl+Shift+U`** path.

**Qt** apps often don’t support that path: type in a **GTK** app first (**Mousepad**), then **copy/paste** into Qt. The **Android soft keyboard** and **Plasma clipboard** are **not integrated**—copy/paste inside Linux using the mouse or shortcuts (e.g. `Ctrl+C`, `Ctrl+V`). You can use a full soft keyboard such as [**Unexpected Keyboard**](https://play.google.com/store/apps/details?id=juloo.keyboard2) ([GitHub](https://github.com/Julow/Unexpected-Keyboard)).

## 7. In-rootfs tuning (`trierarch-optimize`) and Arch documentation

Topic guides for **tuning apps inside the rootfs desktop** are in **[`trierarch-optimize/README.md`](trierarch-optimize/README.md)** ([中文](trierarch-optimize/README.zh.md)), with links to articles (Firefox, non-ASCII input and fonts, etc.).

For **general Arch Linux** setup, packages, and troubleshooting, use **[ArchWiki](https://wiki.archlinux.org/)** and the **[Arch Linux Chinese Wiki](https://wiki.archlinuxcn.org/)**. This app provides **proot + Wayland** on a phone; many Arch steps match a normal desktop install, but kernel features, full **systemd** sessions, etc. may differ.

To build from source, see [`README_DEV.md`](README_DEV.md). **Contributing & security:** [`CONTRIBUTING.md`](CONTRIBUTING.md), [`SECURITY.md`](SECURITY.md); changes in [`CHANGELOG.md`](CHANGELOG.md); releases: [`docs/RELEASING.md`](docs/RELEASING.md).

## Acknowledgments and licenses

This section covers: **thanks to upstreams**, **bundled terminal font licenses**, and **other in-tree code** (see each tree’s `COPYING` / `LICENSE`).

### Thanks

| Thanks to |
|-----------|
| **[PRoot](https://github.com/termux/proot)**; Android build and loader integration under `trierarch-proot/` |
| **Termux**: **[proot-distro](https://github.com/termux/proot-distro)** (Arch rootfs), **[terminal-emulator](https://github.com/termux/termux-app/tree/master/terminal-emulator)** (VT / screen buffer dependency), and the **TerminalView** code under `com.termux.view` derived from Termux |
| **[Wayland](https://wayland.freedesktop.org/)**, **wayland-protocols**; in-app compositor native code under **`trierarch-wayland/`** |
| **[libffi](https://github.com/libffi/libffi)** (Wayland stack), **Rust**, **Kotlin**, **Android NDK / JNI**; host native glue in **`trierarch-native/`** |
| **Jetpack Compose**, **Material 3**, **AndroidX**, **Kotlin Coroutines**, and related Google open-source Android components |
| **GNU/Linux** userland, **[Arch Linux](https://archlinux.org/)** packages and docs (**[ArchWiki](https://wiki.archlinux.org/)**, etc.) |
| **[KDE](https://kde.org/)**, **Plasma**, and sibling free-software communities |
| **AOSP** and fonts such as **Droid Sans Mono** commonly shipped with the platform |
| **[Unexpected Keyboard](https://github.com/Julow/Unexpected-Keyboard)** (third-party keyboard cited in this README; install separately) |
| Upstream font projects listed under **Bundled terminal fonts** below |

### Bundled terminal fonts

In-app shell fonts: open **Appearance** from the floating orb menu. **System monospace** is not shipped under `res/font/`.

| File (`trierarch-app/app/src/main/res/font/`) | Upstream | License |
|-----------------------------------------------|----------|---------|
| `jetbrains_mono_regular.ttf` | [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) | [SIL OFL 1.1](https://openfontlicense.org) |
| `ibm_plex_mono_regular.ttf` | [IBM Plex Mono](https://github.com/googlefonts/ibm-plex) | [SIL OFL 1.1](https://openfontlicense.org) |
| `source_code_pro_regular.ttf` | [Source Code Pro](https://github.com/adobe-fonts/source-code-pro) | [SIL OFL 1.1](https://openfontlicense.org) |
| `noto_sans_mono_regular.ttf` | [Noto Sans Mono](https://github.com/googlefonts/noto-fonts) | [SIL OFL 1.1](https://openfontlicense.org) |
| `droid_sans_mono.ttf` | [Droid Sans Mono](https://cs.android.com/android/platform/frameworks/base/+/master:data/fonts) (AOSP) | [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0) |

**JetBrains Mono** (copyright excerpt from upstream):

> Copyright 2020 The JetBrains Mono Project Authors (https://github.com/JetBrains/JetBrainsMono)
>
> This Font Software is licensed under the SIL Open Font License, Version 1.1.

Other SIL fonts in the table are under OFL as well; full terms and notices are in each upstream repository. **Complete license texts** shipped with the app are in **`assets/licenses/FONT_LICENSES.txt`** (in-tree: `trierarch-app/app/src/main/assets/licenses/FONT_LICENSES.txt`).
