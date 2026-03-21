# trierarch-native

[English](README.md) | 中文

---

[Trierarch](https://github.com/your-org/trierarch-app) 的 Rust JNI 库：负责 proot 启动、PTY 读写及 Arch rootfs 下载/解压，由 Android 应用通过 `NativeBridge` 调用。

## 前置条件

- Rust（rustup）
- Android NDK（`ANDROID_NDK_HOME` 或 `$HOME/Android/Sdk/ndk/` 下）

```bash
rustup target add aarch64-linux-android
```

## 构建

在本仓库根目录设置 NDK 工具链并构建：

```bash
export NDK="${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/25.2.9519653}"   # 或你的 NDK 路径
export AR="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
export CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang"
export CXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang++"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$CC"

cargo build --release --target aarch64-linux-android
```

macOS 上把路径中的 `linux-x86_64` 换成 `darwin-x86_64` 或 `darwin-arm64`。

**产物：** `target/aarch64-linux-android/release/libtrierarch.so`

将该文件拷贝到应用的 jniLibs，供 Android 应用加载：

```bash
cp target/aarch64-linux-android/release/libtrierarch.so /path/to/trierarch-app/app/src/main/jniLibs/arm64-v8a/
```

## 关于脚本

`scripts/build_rust.sh` 适用于同时包含本 crate 与 app 的 workspace。单独使用本仓库时，请按上述 `cargo build` 与 `cp` 步骤操作。
