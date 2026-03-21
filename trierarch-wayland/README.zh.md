# Trierarch Wayland

[English](README.md) | 中文

---

[Trierarch](https://github.com/Beauty114514/trierarch-app) 用的最小化 Wayland 合成器：在 Android 应用进程内运行，通过 SHM 提供单一全屏输出，供 proot 等环境中的 Linux 图形程序在应用 Surface 上显示。**输入**：指针（触摸 → wl_pointer，支持触摸板模式与平板模式）与键盘（Android 软键盘 → wl_keyboard 发给焦点 client）。实现 xdg-shell、presentation-time、viewporter、subcompositor、relative-pointer、pointer-constraints 及最小 data-device/wl_surface 扩展，以便运行完整桌面环境（如 KDE）。

## 前置条件

- Android NDK（如 `$ANDROID_NDK_HOME` 或 `$HOME/Android/Sdk/ndk/<版本>`）
- **面向 Android arm64-v8a 的 Wayland server 与 libffi**  
  本模块依赖 `libwayland-server.so` 和 `libffi.so`，构建前须自行放入 `libs/lib/`。可选方式：
  - **使用自带脚本**（推荐）：在 `trierarch-wayland` 目录下执行  
    `./scripts/build-wayland-android.sh`  
    脚本会编译 libffi、Wayland 和 compositor（ndk-build），并把 **三个** `.so` 都输出到 `out/arm64-v8a/`。需要宿主机安装：`meson`、`ninja`、`wayland`、`wayland-protocols`（如 Arch：`pacman -S meson ninja wayland wayland-protocols`）。脚本会自动查找 NDK，必要时可设置 `ANDROID_NDK_HOME`。
  - 或自行为 Android arm64-v8a 编译 [Wayland](https://gitlab.freedesktop.org/wayland/wayland) 与 [libffi](https://github.com/libffi/libffi)，将 `.so` 与头文件放入 `libs/lib/` 和 `libs/include/`。
  - 或使用预编译包，将 `libwayland-server.so`、`libffi.so` 放入 `libs/lib/`，并保证 `libs/include/` 中有 Wayland server 头文件。

本仓库已包含 `libs/include/` 与 `libs/share/`。脚本会填充 `libs/lib/` 并可刷新 `protocol/`；否则只需从外部构建或预编译得到上述两个 `.so` 并放入 `libs/lib/`。

## 构建

**推荐**：直接执行脚本，会完成全部构建并写入 `out/arm64-v8a/`：

```bash
cd /path/to/trierarch-wayland
./scripts/build-wayland-android.sh
```

**手动构建**：若已有 `libs/lib/` 与 `libs/include/`（wayland + libffi），可只编 compositor：

```bash
cd /path/to/trierarch-wayland
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_ABI=arm64-v8a
```

产物在 `obj/local/arm64-v8a/libwayland-compositor.so`。本模块**仅支持 arm64-v8a**（见 `Application.mk`）。

## 在应用中使用

将 `trierarch-wayland/out/arm64-v8a/` 下的 **所有** `.so` 拷贝到应用的 JNI 库目录：

```bash
cp /path/to/trierarch-wayland/out/arm64-v8a/*.so trierarch-app/app/src/main/jniLibs/arm64-v8a/
```

即得到：`libwayland-compositor.so`、`libwayland-server.so`、`libffi.so`。

在应用内使用 `System.loadLibrary("wayland-compositor")` 加载，并通过 `app.trierarch.WaylandBridge` 调用 JNI：`nativeStartServer`、`nativeSurfaceCreated`、`nativeSurfaceDestroyed`、`nativeStopWayland`、`nativeIsWaylandReady`、`nativeGetOutputSize`、`nativeGetSocketDir`、`nativeOnPointerEvent`、`nativeSetCursorPhysical`、`nativeOnKeyEvent`、`nativeCommitTextUtf8`、`nativeHasActiveClients`。

**多语言输入（Unicode 注入）**：对无法通过普通按键映射产生的字符（如 CJK/emoji），Trierarch 会将提交的字符串按 Unicode codepoint 逐个解析，并注入 Linux 桌面常见的 Unicode 输入序列：`Ctrl+Shift+U` + 十六进制码位 + `Space`（逐字符）。ASCII 与控制键继续走普通键盘事件映射。

## Socket 与运行时目录

合成器在应用传入的运行时目录（如 `getFilesDir()/usr/tmp`）下创建名为 **wayland-trierarch** 的 Wayland socket。客户端（如 proot 内的程序）需将该目录设为 `XDG_RUNTIME_DIR`，并设置 `WAYLAND_DISPLAY=wayland-trierarch` 进行连接。

## 本仓库中的 protocol 与 libs

- **protocol/** — 合成器使用的 Wayland 协议生成的 C 代码（xdg-shell、presentation-time、viewporter、subcompositor 等）。其余协议文件保留供后续扩展。
- **libs/** — Wayland 与 libffi 的头文件及共享库（由脚本或自行填充），见上文前置条件。
- **out/arm64-v8a/** — 统一构建输出：供 app 使用的三个 `.so`。由脚本生成，将其中的文件拷入应用的 jniLibs 即可。
- （已移除）`ime-bridge/` 输入法桥接方案：由于桌面环境对 input-method 协议的暴露/权限策略差异较大，当前主路线使用键盘事件 + Unicode 输入序列注入以获得更广泛的兼容性。

许可证见 Trierarch 仓库根目录。
