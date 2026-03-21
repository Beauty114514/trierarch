# CJK / non-ASCII input in GTK apps (fonts + fcitx5)

> Same as the root [`README.md`](../README.md): **GTK** apps handle `Ctrl+Shift+U` Unicode entry well; **Qt** may be incomplete—use a **GTK app (e.g. Mousepad)** and copy/paste.

## What goes wrong

1. **Missing fonts** — Without CJK fonts you get tofu boxes or broken rendering.  
2. **Input method** — On Plasma (Wayland), without **fcitx5** (or similar) installed and running, some apps never complete the full key → text path. Trierarch still injects `Ctrl+Shift+U`; **GTK** usually consumes it.  
3. **Qt** — Same limitations as in the main README (bridge workflow).

## What to do

### Minimal (fonts + fcitx5)

```bash
sudo pacman -S --needed noto-fonts-cjk wqy-microhei
fcitx5 &
```

(Add `fcitx5` to **Autostart** in System Settings if you want it every session.)

### Full setup (recommended for daily Plasma use)

```bash
sudo pacman -S --needed noto-fonts-cjk noto-fonts fcitx5 fcitx5-chinese-addons fcitx5-configtool
```

In **System Settings → Input / Regional Settings**, set the framework to **Fcitx5** and add Pinyin (or your layout). Check **System Settings → Fonts**; log out/in if needed.

Then in **GTK apps** (Firefox, Mousepad, …): use **fcitx5** switch keys and/or **`Ctrl+Shift+U`** as in the main README.

### Qt apps

If Qt still misbehaves: **type in GTK → copy → paste into Qt**.

## Why this works

- **Fonts** supply glyphs.  
- **fcitx5** provides a normal IME stack; with Trierarch’s injection, **GTK** is the supported path.  
- **Qt** may still need the **GTK workaround**—a known gap, not something this doc removes entirely.
