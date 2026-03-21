# Trierarch — 开发者文档

[用户文档](README.zh.md) | 中文

---

面向需要从源码构建 Android 应用的开发者。下文路径均相对于**仓库根目录**。

应用会从 `trierarch-app/app/src/main/jniLibs/arm64-v8a/` 加载若干原生库。下面按**作用 → 产物如何用 → 详细说明见哪篇文档**来写；**中文**请打开各目录下的 `README.zh.md`，**English** 请打开对应目录的 `README.md`。

---

### Android 应用（界面与 APK）

负责 Compose 界面、资源与 **APK** 打包。需先把 JNI 库放进 `jniLibs`，再在本目录用 Gradle 构建。

- **作用：** 界面与打包、Java/Kotlin 侧逻辑。
- **产物：** 例如 `trierarch-app/app/build/outputs/apk/debug/app-debug.apk`。
- **详见：** [`trierarch-app/README.zh.md`](trierarch-app/README.zh.md)

---

### JNI / 原生层（Rust）

实现 proot 对接、PTY、rootfs 下载与解压等，对应用暴露为 **`libtrierarch.so`**。

- **作用：** Rust JNI，供 Android 代码调用。
- **产物：** `trierarch-native/target/aarch64-linux-android/release/libtrierarch.so` → 复制到 `trierarch-app/app/src/main/jniLibs/arm64-v8a/`。
- **详见：** [`trierarch-native/README.zh.md`](trierarch-native/README.zh.md)

---

### PRoot（Android aarch64）

构建作为应用中 **`libproot.so`** / **`libproot_loader.so`** 使用的 proot 与 loader。

- **作用：** 在 Arch rootfs 内做用户态 chroot/执行环境。
- **产物：** `trierarch-proot/build-android/out/aarch64/` 下二进制 → 按 proot 文档命名复制到 `jniLibs`（见该目录 README）。
- **详见：** [`trierarch-proot/README.zh.md`](trierarch-proot/README.zh.md)

---

### 应用内 Wayland 合成器

构建最小 Wayland server 等 `.so`，使 rootfs 内图形程序能画到应用 Surface 上。

- **作用：** 应用内 Wayland 合成器及相关依赖。
- **产物：** `trierarch-wayland/out/arm64-v8a/*.so` → 全部复制到 `trierarch-app/app/src/main/jniLibs/arm64-v8a/`。
- **详见：** [`trierarch-wayland/README.zh.md`](trierarch-wayland/README.zh.md)

---

## 汇总：放入 JNI 库并打 APK

各部分按各自 `README.zh.md` 构建完成后，在仓库根目录执行：

```bash
mkdir -p trierarch-app/app/src/main/jniLibs/arm64-v8a
cp trierarch-proot/build-android/out/aarch64/proot   trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot.so
cp trierarch-proot/build-android/out/aarch64/loader trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot_loader.so
cp trierarch-native/target/aarch64-linux-android/release/libtrierarch.so trierarch-app/app/src/main/jniLibs/arm64-v8a/
cp trierarch-wayland/out/arm64-v8a/*.so trierarch-app/app/src/main/jniLibs/arm64-v8a/
```

确认 `trierarch-app/app/src/main/assets/keymap_us.xkb` 存在（本仓库已包含）。然后：

```bash
cd trierarch-app
./gradlew assembleDebug
./gradlew installDebug
```

输出：`trierarch-app/app/build/outputs/apk/debug/app-debug.apk`。

## 备注

- 应用将 Arch rootfs 放在 `data_dir/arch`（内部存储）。若首次启动不存在，可能通过 `proot-distro` 自动下载（需网络）。
