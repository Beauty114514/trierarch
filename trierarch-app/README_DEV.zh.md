# Trierarch（应用）- 开发者文档

[用户文档](README.zh.md) | 中文

---

本文件面向需要从源码构建 Android APK 的开发者。

## 前置条件

- Android Studio（或 SDK + NDK）
- 来自 **trierarch-proot** 的构建产物（`libproot.so`、`libproot_loader.so`）以及来自 **trierarch-native** 的构建产物（`libtrierarch.so`）。
- 如果需要使用应用内置的 Wayland 视图：请参见下方 Wayland 视图小节。

## Wayland 视图小节（trierarch-wayland）

要启用应用内置的 Wayland 合成器界面：

1. 构建 **trierarch-wayland**；
2. 将 `out/arm64-v8a/` 下的所有 `.so` 拷贝到 `app/src/main/jniLibs/arm64-v8a/`；
3. 确保 APK 中包含 `app/src/main/assets/keymap_us.xkb`（本仓库已打包）。

## 构建

1. 获取 **trierarch-proot** 的 proot 库（见该仓库 README），复制到本仓库：

   ```bash
   mkdir -p app/src/main/jniLibs/arm64-v8a
   cp /path/to/trierarch-proot/build-android/out/aarch64/proot   app/src/main/jniLibs/arm64-v8a/libproot.so
   cp /path/to/trierarch-proot/build-android/out/aarch64/loader  app/src/main/jniLibs/arm64-v8a/libproot_loader.so
   ```

2. 获取 **trierarch-native** 的 `libtrierarch.so`（见该仓库 README），复制到本仓库：

   ```bash
   cp /path/to/trierarch-native/target/aarch64-linux-android/release/libtrierarch.so app/src/main/jniLibs/arm64-v8a/
   ```

3. 在本仓库根目录构建 APK：

   ```bash
   ./gradlew assembleDebug
   ./gradlew installDebug
   ```

输出路径：`app/build/outputs/apk/debug/app-debug.apk`。

## 备注

- 应用将 Arch rootfs 存放在 `data_dir/arch`（应用内部存储）。若首次启动时不存在，则会通过 `proot-distro` 自动下载（需要网络）。

