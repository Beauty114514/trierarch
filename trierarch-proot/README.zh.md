# trierarch-proot

[English](README.md) | 中文

[Trierarch monorepo](https://github.com/Beauty114514/trierarch) 中的 proot 构建目录。基于 [termux/proot](https://github.com/termux/proot)。上游版权与许可说明见本目录 **`COPYING`**。

## 作用与已实现能力

为 **Android arm64-v8a** 交叉编译 **PRoot** 及其 **loader**。Android 应用将其作为 native 库加载（`libproot.so`、`libproot_loader.so` 等），用于在设备上运行 Arch rootfs 环境。

## 前置条件

- Android NDK（`ANDROID_NDK_HOME` 或 `$HOME/Android/Sdk/ndk/`）
- 本地构建所需：`make`、`wget`、`tar`、`bash`
- **Docker**（可选，用于容器化脚本构建）

## 手动构建

在 **`trierarch-proot/build-android/`** 下：

```bash
cd trierarch-proot/build-android
./get-talloc.sh
./make-talloc-static.sh
./make-proot.sh
```

若 NDK 不在脚本默认位置，请设置 `NDK=/path/to/ndk`。

**产物：** `trierarch-proot/build-android/out/aarch64/proot`、`loader`，以及可能存在的 `loader32`。

## 脚本构建（Docker）

在 **monorepo 根目录**执行（推荐，便于与 CI 一致或避免宿主机 Python 2 等差异）：

```bash
./trierarch-proot/build-android/docker-build.sh
# 或指定 NDK：
./trierarch-proot/build-android/docker-build.sh /path/to/ndk
```

会在 `build-android/` 下构建 Docker 镜像，将整个 monorepo 挂载到 `/workspace`，在 **`trierarch-proot/build-android`** 内执行 `./build.sh`，输出到 `trierarch-proot/build-android/out/aarch64/`。

也可手动执行与上述等价的 `docker build` + `docker run`（工作目录需为 `/workspace/trierarch-proot/build-android`，见 `docker-build.sh`）。

## 编译产物怎么用

在 **monorepo 根目录**执行拷贝并重命名（供应用加载）：

```bash
mkdir -p trierarch-app/app/src/main/jniLibs/arm64-v8a
cp trierarch-proot/build-android/out/aarch64/proot   trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot.so
cp trierarch-proot/build-android/out/aarch64/loader trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot_loader.so
# 若存在：
# cp trierarch-proot/build-android/out/aarch64/loader32 trierarch-app/app/src/main/jniLibs/arm64-v8a/libproot_loader32.so
```

然后在 `trierarch-app/` 构建 APK；完整步骤见 [`README_DEV.zh.md`](../README_DEV.zh.md)。
