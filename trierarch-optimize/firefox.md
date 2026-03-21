# Firefox (Trierarch / proot)

Install and run Firefox inside the Trierarch Arch environment. Without the config changes below, Firefox may crash.

## 1. Install Firefox

In the Trierarch terminal (proot shell), run:

```bash
pacman -S firefox
```

Confirm with `Y` when prompted. If you need to initialize the keyring first:

```bash
pacman-key --init
pacman-key --populate archlinux
```

Then run `pacman -S firefox` again.

## 2. Fix crashes (about:config)

Firefox can crash in this environment due to sandbox settings. Adjust them as follows.

1. **Open config**
   - In the Firefox address bar, type: `about:config`
   - Press **Enter**.

2. **Accept warning**
   - Click **“Accept the Risk and Continue”** to open the preferences (about:config) page.

3. **Search and change settings**
   - In the **Search** box on the about:config page, type: `sandbox`
   - Find and set:
     - **`media.cubed.sandbox`** → set to **`false`**
     - **`security.sandbox.content.level`** → set to **`0`**
   - (Double-click the value to change it, or use the edit control.)

4. **Restart**
   - Restart Firefox. It should then run normally in Trierarch.

## Summary

| Setting                         | New value |
|---------------------------------|-----------|
| `media.cubed.sandbox`           | `false`   |
| `security.sandbox.content.level`| `0`       |
