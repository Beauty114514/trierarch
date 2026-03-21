# Android 应用

[English](README.md) | 中文

[Trierarch monorepo](https://github.com/Beauty114514/trierarch) 中的应用子目录。

## 作用与已实现能力

- **Jetpack Compose** 界面：安装流程、终端、侧边栏、Wayland 视图开关、Display 启动脚本编辑、设置等。
- **资源与集成：** proot 启动脚本、键位表、JNI 库名等；**`jniLibs/arm64-v8a/`** 加载由其它目录构建的 `.so`（Rust JNI、proot、Wayland 等）。
- **桥接：** Kotlin `NativeBridge` / `WaylandBridge` 调用 native 层。

本目录**不单独编译**上述原生库；依赖与拷贝顺序见仓库根目录 [`README_DEV.zh.md`](../README_DEV.zh.md) 及各组件 README。

## 前置条件

- Android Studio 或 Android SDK + 合适的 **NDK**（供 Gradle 使用）。
- 已按 [`README_DEV.zh.md`](../README_DEV.zh.md) 将预编译 `.so` 放入 `app/src/main/jniLibs/arm64-v8a/`。

## 手动构建（Gradle）

在 monorepo 中进入 **`trierarch-app/`**：

```bash
cd trierarch-app
./gradlew assembleDebug
```

可选：安装到已连接设备：

```bash
./gradlew installDebug
```

**产物示例：** `app/build/outputs/apk/debug/app-debug.apk`。

除 **`gradlew`** 外无额外封装脚本；日常构建即使用上述命令。

## 编译产物怎么用

- 在 **arm64-v8a** 设备/模拟器上安装 APK。
- 运行时期望 `jniLibs` 下已有对应 `.so`，Arch rootfs 等行为见根目录 [`README.zh.md`](../README.zh.md)。

在执行 `./gradlew` 之前如何汇总 JNI 库，见 [`README_DEV.zh.md`](../README_DEV.zh.md)。
