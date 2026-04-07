# trierarch-audio

[English](README.md) | 中文

[Trierarch monorepo](https://github.com/Beauty114514/trierarch) 中的宿主 PulseAudio 集成。设备上前缀 **`filesDir/pulse/`**（`bin/pulseaudio`、`lib/`…）；guest 使用 **`libpulse`** 通过 **Unix socket** 连接（`PULSE_SERVER=unix:/run/trierarch-pulse/native`）；由 **`trierarch-native`** `pulse_host` 拉起 daemon（AAudio 输出）。

## 目录约定（哪些会进仓库）

- **会被追踪（本仓库维护）**：`build-android/`、`pulse-android/`、`README*.md`、`.gitignore`
- **脚本生成（已忽略）**：`third_party/`（上游 clone）、`out/`（sysroot 与 install 前缀）

## 前置条件

- **git**（克隆上游）
- 交叉编译阶段：**Android NDK**、**meson**、**ninja**（及你愿意启用的依赖）

## 手动构建

**1.** 克隆上游并与本仓库端口合并（补丁针对 **v17.0**）：

```bash
git clone --depth 1 --branch v17.0 https://github.com/pulseaudio/pulseaudio.git
export PULSE_SRC="$(pwd)/pulseaudio"
cd trierarch-audio/build-android
./build-pulse.sh
```

**2.** 在 **`$PULSE_SRC`** 中用 **NDK + Meson** 编译并 **`meson install`**，安装前缀的目录结构需与设备上 **`getFilesDir()/pulse/`** 一致（Meson 选项可参考 Termux **`packages/pulseaudio`**）。

改 **`pulse-android/`** 补丁后重打：在克隆目录 **`git reset --hard && git clean -fdx`**，再 **`FORCE_INTEGRATE=1 ./build-pulse.sh`**（或删除 **`third_party/pulseaudio`** 后改走脚本构建）。

无声：在 **`default.pa`** 或 **`pulse_host.rs`** 中为 SLES/AAudio 增加 **`load-module`**。

## 脚本构建

默认 clone 到 **`trierarch-audio/third_party/pulseaudio`**：

```bash
cd trierarch-audio/build-android
./build-pulse-android.sh
```

`build-pulse-android.sh` 会构建 sysroot 依赖、跑 Meson 交叉编译并 install 到 **`trierarch-audio/out/pulse-android-prefix`**，然后把前缀 **同步到应用 assets**：**`trierarch-app/app/src/main/assets/pulse/`**（可用 `PULSE_SKIP_INSTALL_TO_APP=1` 关闭同步）。

## 编译产物怎么用

将 **`meson install`** 得到的前缀（至少含 **`bin/pulseaudio`** 与依赖 **`lib/`**）放到应用 **`getFilesDir()/pulse/`**（或与 `pulse_host` 约定的其它搜索路径）。详见 **`trierarch-native`** 源码中的 **`pulse_host`**。

| 路径 | 说明 |
|------|------|
| `pulse-android/` | 补丁、`modules/*.c` |
| `build-android/` | **`build-pulse-android.sh`**（clone+集成）、`build-pulse.sh`（仅集成，需已有 **`PULSE_SRC`**） |
