# trierarch-optimize

[中文](README.zh.md) | English

---

This directory collects **workarounds for issues tied to Trierarch’s own stack**, including:

- The **in-app Wayland compositor** and input path (e.g. `Ctrl+Shift+U` injection) vs **GTK/Qt** behaviour;  
- **proot** (and similar Termux-style user-space containers): namespaces, filesystem layout, `/proc`, **inotify**, etc. differ from a real machine.

**If your issue is generic Arch, KDE, or app configuration on a normal desktop**, start with the **[ArchWiki](https://wiki.archlinux.org/)** and upstream docs. Here we only add notes for problems that show up **more often** under **Trierarch + proot + this compositor**.

## Guides (template: cause → fix → why it works)

| Topic | Document |
|-------|----------|
| Firefox crashing under proot | [`firefox.md`](firefox.md) |
| CJK input in GTK apps (fonts + fcitx5) | [`input-cjk-gtk.md`](input-cjk-gtk.md) |
| Baloo / `tags:/` pop-ups in KDE | [`baloo-tags-warning.md`](baloo-tags-warning.md) |

(Add new rows as you write more guides; keep the same template.)
