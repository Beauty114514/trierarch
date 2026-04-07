# Trierarch

[中文](README.zh.md) | English

---

**Download:** latest APK → **[GitHub Releases](https://github.com/Beauty114514/trierarch/releases/latest)**.

## Demo

| KDE Plasma (Wayland) | VS Code (inside proot) |
|------------------------|-------------------------|
| ![KDE Plasma desktop](docs/images/desktop.jpg) | ![VS Code](docs/images/vscode.jpg) |

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
pacman -S plasma-meta dolphin konsole
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

**Audio note (PulseAudio):** Trierarch runs a host PulseAudio daemon (AAudio sink) and exposes it to the proot desktop over a Unix socket. To avoid "silent but playing" cases caused by the default **Null Output**, Trierarch will **auto-inject** a small snippet into your saved Display startup script that waits for PulseAudio and runs `pactl set-default-sink trierarch-out` (idempotent; it won’t be inserted twice).

Even in a **proot** environment, it is good practice to **run the desktop as a normal user**, not as root. If you want to dig deeper, read ArchWiki **[Users and groups](https://wiki.archlinux.org/title/Users_and_groups)** and **[Sudo](https://wiki.archlinux.org/title/Sudo)**; otherwise the short recipe below is enough.

**Create a user and sudo (example user `myuser`):**

```bash
pacman -S sudo
useradd -m -G wheel -s /bin/bash myuser
passwd myuser
EDITOR=nano visudo
```

In the editor, you can either **uncomment** the line `%wheel ALL=(ALL:ALL) ALL` so everyone in group `wheel` may use `sudo`, **or** leave that line as-is and **add below it** a dedicated rule, e.g. `myuser ALL=(ALL:ALL) ALL`. Alternatively, add a file under `/etc/sudoers.d/` (same syntax; use `visudo -f /etc/sudoers.d/myuser` to avoid syntax errors). Save and exit.

In the **Display** script editor (long-press **Display**), start Plasma **as that user** so files under `/home/myuser` belong to the session: **first line** switch to the account, **Enter**, **second line** the startup command. See the screenshot below.

![Display in orb menu](docs/images/displayScript.jpg)

**Sessions** (orb menu) lists terminal tabs—switch, add, or remove sessions.

![Sessions](docs/images/sessionSettings.jpg)

**Appearance** (orb menu): customize the in-app **Terminal** look—**fonts only** for now. For further beautification or theming ideas, **open an issue**—reasonable suggestions welcome.

![Appearance settings](docs/images/appearenceSettings.jpg)

## 4. Daily use: tap app to auto-start the desktop

After you have set a Display startup script, you can **tap the app icon normally** to enter the desktop view; the app will **inject and run the Display script automatically** (idempotent when a desktop client is already connected).

The **Terminal** shortcut is for **first-time setup** (e.g. installing the desktop and terminal packages) or when you **prefer the in-app native terminal**—it’s optimized for **smooth switching** between the **desktop and terminal** views.

## 5. After Plasma starts: View Settings and terminal ↔ desktop switching

### View Settings

Open **View Settings** from the orb menu to tune the compositor and pointer behavior:

![View Settings](docs/images/viewSettings.jpg)

- **Pointer / mouse mode** — e.g. **touchpad-style (relative)** vs **tablet-style (absolute)**.
- **UI size vs clarity** — **Resolution** and **Scale** can be **combined**: Resolution lowers output resolution to reduce compositing load **and** adjusts on-screen element size; Scale resizes via scaling without changing the backing resolution to keep text/UI sharp. There’s no single “correct” mix—tune until it fits **your** device and habits.

### Switching between terminal and desktop with Display

After Plasma is running, tap **Display** in the orb menu again to return to the **terminal / Wayland** view; **open the orb menu → Display** to go back to the desktop. The orb menu and Display work in both the shell view and on the desktop.

## 6. In-rootfs tuning (`trierarch-optimize`)

For topic guides (browser, IME, fonts, etc.) after you’re on the desktop, see **[`trierarch-optimize/README.md`](trierarch-optimize/README.md)** ([中文](trierarch-optimize/README.zh.md)).

**Strongly recommended:** fix **Baloo / virtual `tags:/`** issues common under **proot** (otherwise repeated pop-ups and wasted resources). Follow **[Baloo and `tags:/`](trierarch-optimize/baloo-tags-warning.md)** ([中文](trierarch-optimize/baloo-tags-warning.zh.md)).

For general Arch setup and troubleshooting, use **[ArchWiki](https://wiki.archlinux.org/)** and the **[Arch Linux Chinese Wiki](https://wiki.archlinuxcn.org/)**. This app is **proot + Wayland**; some full-desktop assumptions (kernel, systemd sessions, …) may not apply.

## 7. Keyboard and input (orb menu Keyboard, GTK / Qt)

The **Keyboard** item in the orb menu can **invoke** the soft keyboard.

![Keyboard in orb menu](docs/images/keyboard.jpg)

In **GTK** apps you can enter **non-ASCII** characters (Chinese, emoji, special characters, etc.); input is completed automatically via the **`Ctrl+Shift+U`** path.

**Qt** apps often don’t support that path: type in a **GTK** app first (**Mousepad**), then **copy/paste** into Qt. The **Android soft keyboard** and **Plasma clipboard** are **not integrated**—copy/paste inside Linux using the mouse or shortcuts (e.g. `Ctrl+C`, `Ctrl+V`). You can use a full soft keyboard such as [**Unexpected Keyboard**](https://play.google.com/store/apps/details?id=juloo.keyboard2) ([GitHub](https://github.com/Julow/Unexpected-Keyboard)).

## 8. Build, changes, and releases

To build from source, see [`README_DEV.md`](README_DEV.md). **Contributing & security:** [`CONTRIBUTING.md`](CONTRIBUTING.md), [`SECURITY.md`](SECURITY.md); changes in [`CHANGELOG.md`](CHANGELOG.md); releases: [`docs/RELEASING.md`](docs/RELEASING.md).

## 9. Acknowledgments and licenses

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
