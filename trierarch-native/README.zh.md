# trierarch-native

[English](README.md) | 中文

[Trierarch monorepo](https://github.com/Beauty114514/trierarch) 中的 Rust JNI 子目录。

## 作用与已实现能力

Rust 编译为 **`cdylib`**，在 Android 侧以 **`libtrierarch.so`** 加载（`System.loadLibrary("trierarch")`），JNI 由 `NativeBridge` 调用。

**已实现（概览）：** 与 proot 相关的进程/PTY、Arch rootfs 下载与校验、分阶段解压与原子安装到应用存储、PTY 输出缓冲等 Kotlin 所需能力。

## 前置条件

- [Rust](https://www.rust-lang.org/)（`rustup`）
- Android NDK（`ANDROID_NDK_HOME` 或 `$HOME/Android/Sdk/ndk/`）

```bash
rustup target add aarch64-linux-android
```

## 手动构建

在 **`trierarch-native/`** 下为 Cargo 指定 NDK 工具链（macOS 上将 `linux-x86_64` 换成 `darwin-x86_64` 或 `darwin-arm64`）：

```bash
cd trierarch-native
export NDK="${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/25.2.9519653}"   # 你的 NDK 路径
export AR="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
export CC="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang"
export CXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang++"
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$CC"

cargo build --release --target aarch64-linux-android
```

**产物：** `target/aarch64-linux-android/release/libtrierarch.so`

## 脚本构建

在 **`trierarch-native/`** 执行：

```bash
./scripts/build_rust.sh
```

脚本会自动查找 NDK、设置工具链、执行 `cargo build --release --target aarch64-linux-android`，并将

`target/aarch64-linux-android/release/libtrierarch.so` 复制到 `../trierarch-app/app/src/main/jniLibs/arm64-v8a/`。

**依赖** 标准 monorepo 目录结构（`trierarch-app` 与 `trierarch-native` 同级）。

## 编译产物怎么用

若使用手动构建，请将库拷贝到应用模块旁：

```bash
cp target/aarch64-linux-android/release/libtrierarch.so ../trierarch-app/app/src/main/jniLibs/arm64-v8a/
```

然后在 `trierarch-app/` 打 APK，完整集成步骤见 [`README_DEV.zh.md`](../README_DEV.zh.md)。
