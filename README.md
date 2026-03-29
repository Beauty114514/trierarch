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

- **Floating menu orb** — A draggable **orb** on screen; **short tap** opens a **glass menu** (Display, View Settings, Keyboard, and other actions). Drag the orb to move it; position is remembered between sessions.

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

## Acknowledgments

Trierarch stands on many open-source projects. Thanks to their authors and communities, including (non-exhaustively):

- **[PRoot](https://github.com/termux/proot)** and the **Termux** ecosystem; Arch rootfs from **[proot-distro](https://github.com/termux/proot-distro)** releases
- **[Wayland](https://wayland.freedesktop.org/)** and **wayland-protocols** (under `trierarch-wayland/`)
- **[libffi](https://github.com/libffi/libffi)** · **Rust** · **Kotlin** · **Jetpack Compose** · **Android** / **NDK**
- The **KDE Plasma** and **Arch Linux** communities
- **[Unexpected Keyboard](https://github.com/Julow/Unexpected-Keyboard)** (third-party keyboard mentioned in this README)

Bundled third-party code includes its own license files in-tree (e.g. `COPYING`, `LICENSE` under vendored paths). This section is a thank-you, not a complete legal notice.
