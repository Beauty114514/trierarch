# Trierarch

[English](README.md) | 中文

---

本仓库为 **monorepo**，包含 Android 应用及相关组件（`trierarch-app`、`trierarch-native`、`trierarch-proot`、`trierarch-wayland`、`trierarch-optimize` 等）。主仓库地址：**[github.com/Beauty114514/trierarch](https://github.com/Beauty114514/trierarch)**。

**Trierarch** 是一款 Android 应用：采用与 **Termux** 生态一致的 **proot** 容器技术（**无需**单独安装 Termux 应用），在设备上运行 **Arch Linux** rootfs。其他桌面环境在**原理上**也可尝试或实现（同样的 proot + Wayland 链路），但**开发、文档与体验优化重心在 KDE Plasma（Wayland）**。应用内置 **Wayland** 合成器（触摸指针、绝对/相对模式、软键盘输入等）。在应用内通过 **Display 启动脚本** 启动你的 **Plasma（Wayland）** 会话。界面为 Jetpack Compose；JNI/native 层见 [`trierarch-native/`](trierarch-native/)。

**桌面内软件与体验优化：** 在 rootfs 桌面环境中优化软件、性能与使用习惯的说明与教程，从 [`trierarch-optimize/README.zh.md`](trierarch-optimize/README.zh.md) 读起（[English](trierarch-optimize/README.md)）；该索引会链接到各篇中文优化教程（如 Firefox）。

如需从源码构建应用，请查看 [`README_DEV.zh.md`](README_DEV.zh.md)。

**贡献与安全：**见 [`CONTRIBUTING.zh.md`](CONTRIBUTING.zh.md)（[English](CONTRIBUTING.md)）、[`SECURITY.zh.md`](SECURITY.zh.md)（[English](SECURITY.md)）。变更记录见 [`CHANGELOG.md`](CHANGELOG.md)。

## Arch rootfs

- rootfs 路径：`data_dir/arch`（应用内部存储）。
- **自动下载**：若首次启动时不存在 rootfs，应用会从 [proot-distro](https://github.com/termux/proot-distro/releases) 下载 Arch Linux aarch64 rootfs（约 156 MB）并解压，需联网。
- **手动**：也可自行下载 Termux proot-distro 的 Arch aarch64 rootfs 解压到该目录。
- 若 `data_dir/arch` 下没有可用的 `sh`，proot 会以 `-0 /system/bin/sh` 运行。

## Wayland 与 Display

- 在侧边栏开启 **Wayland**，切换到 Wayland 视图即可看到合成器画面。触摸作为指针（设置中可选绝对/相对触摸板模式）；点击 Keyboard 将按键发给当前焦点 client。
- **Display**：点击可执行你配置的 **Display 启动脚本**（请按 **KDE Plasma / Wayland** 的方式编写）；长按可编辑脚本。若已有 Wayland 客户端连接，应用不会重复执行脚本。

## 输入说明（GTK / Qt）

我们目前在 **GTK 类应用**里已经解决了 `Ctrl+Shift+U` 的输入问题：可在这类应用中更流畅地输入中文、其他语言字符以及 Emoji（例如 Firefox 等）。

由于底层限制，**Qt 类应用**对 `Ctrl+Shift+U` 的支持可能不完整，因此在 Qt 应用里如果有中文/非 ASCII 输入需求，建议：

- 先在支持 `Ctrl+Shift+U` 的 **GTK 应用**（例如 Mousepad）中完成输入；
- 然后复制粘贴到 Qt 应用中（通常 `Ctrl+V` / `Ctrl+Shift+V` 在目标应用里行为略有差异，以实际应用表现为准）。

如果你偏好使用更接近“全键盘”的软键盘，建议尝试 [**Unexpected Keyboard**](https://play.google.com/store/apps/details?id=juloo.keyboard2)（[Google Play 官方应用页](https://play.google.com/store/apps/details?id=juloo.keyboard2)；**开源**，[GitHub 仓库](https://github.com/Julow/Unexpected-Keyboard)），可更方便地输入/复制 ASCII 以及其他字符。

未来如果我们找到更通用的解决思路，会尽快在开发版本中更新；也欢迎你或其他开源开发者提供可行方案与测试反馈。由于个人精力有限，暂时难以在所有场景上都做到一次到位，感谢你的理解与协助。

## 致谢

Trierarch 依赖大量开源软件，向相关项目作者与社区致谢（**列举不全**）：

- **[PRoot](https://github.com/termux/proot)** 与 **Termux** 生态；Arch rootfs 来自 **[proot-distro](https://github.com/termux/proot-distro)** 发布包
- **[Wayland](https://wayland.freedesktop.org/)** 与 **wayland-protocols**（参考源码位于 `trierarch-wayland/` 下）
- **[libffi](https://github.com/libffi/libffi)** · **Rust** · **Kotlin** · **Jetpack Compose** · **Android** / **NDK**
- **KDE Plasma** 与 **Arch Linux** 社区（文档与体验优化的主要目标环境）
- 本文档推荐的第三方软键盘 **[Unexpected Keyboard](https://github.com/Julow/Unexpected-Keyboard)**

随仓库打包的上游代码在各自目录中保留许可证文件（如 `COPYING`、`LICENSE` 等）。本小节为致谢与指路，**不构成**完整法律声明；以仓库内实际文件与上游条款为准。
