# trierarch-proot

[English](README.md) | 中文

---

为 [Trierarch](https://github.com/your-org/trierarch-app) 构建的 Android 用 proot（aarch64）。基于 [termux/proot](https://github.com/termux/proot)（Termux 使用的修补版 PRoot）。

## 前置条件

- Android NDK（`ANDROID_NDK_HOME` 或 `$HOME/Android/Sdk/ndk/`）
- [Docker](https://docs.docker.com/get-docker/)（可选，用于脚本化构建）

## 构建

### 方式一：Docker（推荐）

在本仓库根目录执行：

```bash
NDK="${ANDROID_NDK_HOME:-$HOME/Android/Sdk/ndk/25.2.9519653}"   # 或你的 NDK 路径
docker build -t trierarch-proot-build build-android
docker run --rm -v "$PWD:/workspace" -v "$NDK:/opt/ndk" -e NDK=/opt/ndk -w /workspace/build-android trierarch-proot-build ./build.sh
```

或使用自定义 NDK 路径：

```bash
./build-android/docker-build.sh /path/to/ndk
```

（若 `docker-build.sh` 是按父仓库目录结构写的，请使用上面的 `docker build` + `docker run` 命令。）

**产物：** `build-android/out/aarch64/proot`、`build-android/out/aarch64/loader`。

### 方式二：本地构建

在 `build-android/` 目录下：

```bash
cd build-android
./get-talloc.sh
./make-talloc-static.sh
./make-proot.sh
```

需安装：NDK、make、wget、tar、bash。若 NDK 不在默认路径，可设置 `NDK=/path`。

## 在应用中使用

将构建出的二进制拷贝到 trierarch-app 仓库中作为 native 库（应用会从 lib 目录加载）：

```bash
mkdir -p /path/to/trierarch-app/app/src/main/jniLibs/arm64-v8a
cp build-android/out/aarch64/proot   /path/to/trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot.so
cp build-android/out/aarch64/loader  /path/to/trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot_loader.so
```
