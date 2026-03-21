# trierarch-wayland

[English](README.md) | 中文

[Trierarch monorepo](https://github.com/Beauty114514/trierarch) 中的 Wayland 合成器目录。目录内自带的 **Wayland** / **wayland-protocols** / **libffi** 上游树请保留其自带的许可证文件。

## 作用与已实现能力

在 Android 应用进程内运行的**最小 Wayland 合成器**：单一全屏 SHM 输出，使 proot 等环境中的 Linux 图形客户端能画到应用 **Surface**。**输入：** 触摸 → `wl_pointer`（含绝对/相对/触摸板模式），软键盘 → `wl_keyboard`。**协议：** xdg-shell、presentation-time、viewporter、subcompositor、relative-pointer、pointer-constraints 及最小 data-device / `wl_surface` 相关实现，足以支撑 **KDE Plasma（Wayland）** 等桌面栈。

**Unicode 输入：** 对 CJK/emoji 等无法直接映射的字符，将提交文本按码位拆分，并注入常见的 `Ctrl+Shift+U` + 十六进制 + `Space` 序列（逐码位）；ASCII 仍走普通按键事件。

**运行时：** 在应用提供的目录（如 `getFilesDir()/usr/tmp`）下创建名为 **`wayland-trierarch`** 的 socket；客户端需设置 `XDG_RUNTIME_DIR` 与 `WAYLAND_DISPLAY=wayland-trierarch`。

## 前置条件

- Android NDK（`ANDROID_NDK_HOME` 或 SDK 下 `ndk/<版本>`）
- **仅手动编 compositor** 时：需已有面向 **arm64-v8a** 的 `libwayland-server.so`、`libffi.so` 及头文件（本仓库已含 `libs/include/`、`libs/share/`）。
- **一键脚本** 时：宿主机需 **meson**、**ninja**、**wayland**、**wayland-protocols**（如 Arch：`pacman -S meson ninja wayland wayland-protocols`）。

本模块**仅 arm64-v8a**（见 `Application.mk`）。

## 手动构建

在已准备好 **`libs/lib/`**（含 `libwayland-server.so`、`libffi.so` 及头文件）时，在 **`trierarch-wayland/`** 下只编合成器：

```bash
cd trierarch-wayland
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_ABI=arm64-v8a
```

**中间产物：** `obj/local/arm64-v8a/libwayland-compositor.so`。依赖库仍需与之一同打包；日常更推荐下面**脚本构建**，一次性输出到 `out/arm64-v8a/`。

## 脚本构建（推荐）

编译 **libffi**、**Wayland** 与合成器，并将全部 `.so` 输出到 **`out/arm64-v8a/`**：

```bash
cd trierarch-wayland
./scripts/build-wayland-android.sh
```

需安装上文宿主机依赖；若找不到 NDK，请设置 `ANDROID_NDK_HOME`。

**产物：** `trierarch-wayland/out/arm64-v8a/libwayland-compositor.so`、`libwayland-server.so`、`libffi.so`。

## 编译产物怎么用

将 `out/arm64-v8a/` 下**所有** `.so` 拷贝到应用 JNI 目录（在 monorepo 根目录执行）：

```bash
cp trierarch-wayland/out/arm64-v8a/*.so trierarch-app/app/src/main/jniLibs/arm64-v8a/
```

应用通过 `System.loadLibrary("wayland-compositor")` 加载，并在 `app.trierarch.WaylandBridge` 上调用 JNI（`nativeStartServer`、`nativeSurfaceCreated`、`nativeSurfaceDestroyed`、`nativeStopWayland`、`nativeIsWaylandReady`、`nativeGetOutputSize`、`nativeGetSocketDir`、`nativeOnPointerEvent`、`nativeSetCursorPhysical`、`nativeOnKeyEvent`、`nativeCommitTextUtf8`、`nativeHasActiveClients` 等）。

随后在 `trierarch-app/` 构建 APK；完整集成见 [`README_DEV.zh.md`](../README_DEV.zh.md)。

## 本目录结构说明

- **`protocol/`** — 合成器使用的协议生成代码。
- **`libs/`** — 头文件与（脚本或手动步骤后）共享库。
- **`out/arm64-v8a/`** — 打包进 APK 的统一输出目录。
- **`wayland/`**、**`wayland-protocols/`**、**`libffi/`** — 上游源码，勿删其中许可证文件。

**本仓库合成器源码**的许可证见 monorepo 根目录 [`LICENSE`](../LICENSE)。
