# Trierarch（应用）

[English](README.md) | 中文

---

Android 应用：通过 **proot** 在无 Termux 环境下运行 Linux shell（可选 Arch rootfs）。可选 **Wayland** 图形：内置合成器，支持指针（触摸、绝对/相对模式）与键盘（软键盘 → Wayland）。通过 **Display 启动脚本**（应用内设置）可在 proot 内启动任意桌面或窗口管理器（如 sway、Xfce、KDE）。Jetpack Compose UI；native 层来自 [trierarch-native](https://github.com/Beauty114514/trierarch-native)。

如需从源码构建应用，请查看 [`README_DEV.zh.md`](README_DEV.zh.md)。

## Arch rootfs

- rootfs 路径：`data_dir/arch`（应用内部存储）。
- **自动下载**：若首次启动时不存在 rootfs，应用会从 [proot-distro](https://github.com/termux/proot-distro/releases) 下载 Arch Linux aarch64 rootfs（约 156 MB）并解压，需联网。
- **手动**：也可自行下载 Termux proot-distro 的 Arch aarch64 rootfs 解压到该目录。
- 若 `data_dir/arch` 下没有可用的 `sh`，proot 会以 `-0 /system/bin/sh` 运行。

## Wayland 与 Display

- 在侧边栏开启 **Wayland**，切换到 Wayland 视图即可看到合成器画面。触摸作为指针（设置中可选绝对/相对触摸板模式）；点击 Keyboard 将按键发给当前焦点 client。
- **Display**：点击可执行你配置的 **Display 启动脚本**（如 `sway` 或 `startx`）；长按可编辑脚本。若已有 Wayland 客户端连接，应用不会重复执行脚本。

## 输入说明（GTK / Qt）

我们目前在 **GTK 类应用**里已经解决了 `Ctrl+Shift+U` 的输入问题：可在这类应用中更流畅地输入中文、其他语言字符以及 Emoji（例如 Firefox 等）。

由于底层限制，**Qt 类应用**对 `Ctrl+Shift+U` 的支持可能不完整，因此在 Qt 应用里如果有中文/非 ASCII 输入需求，建议：

- 先在支持 `Ctrl+Shift+U` 的 **GTK 应用**（例如 Mousepad）中完成输入；
- 然后复制粘贴到 Qt 应用中（通常 `Ctrl+V` / `Ctrl+Shift+V` 在目标应用里行为略有差异，以实际应用表现为准）。

如果你偏好使用更接近“全键盘”的软键盘，建议尝试 **Unexpected Keyboard**（可更方便地输入/复制 ASCII 以及其他字符）。

未来如果我们找到更通用的解决思路，会尽快在开发版本中更新；也欢迎你或其他开源开发者提供可行方案与测试反馈。由于个人精力有限，暂时难以在所有场景上都做到一次到位，感谢你的理解与协助。
