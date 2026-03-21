# 参与 Trierarch 贡献

[English](CONTRIBUTING.md) | 中文

感谢你有兴趣参与本项目。本文档是**起步说明**，欢迎通过 PR 一起改进。

## 沟通方式

- **Issue / PR** 可使用 **中文或 English**（或混用），选你顺手的语言即可。
- **较大改动**（新功能、重构、接口变更）建议先开 **Issue** 对齐方向，再动手。

## 贡献流程

1. **Fork** 仓库，从 **`main`** 拉出分支。
2. 提交尽量 **聚焦、信息明确**（例如 `fix(native): …`、`docs: …`、`feat(app): …`——推荐 [Conventional Commits](https://www.conventionalcommits.org/)，但不强制）。
3. 对 **`main`** 开 **Pull Request**，说明 **做了什么、为什么**。
4. 若涉及 **构建或文档**，请同步改相关 `README` / `README_DEV`，保证别人仍能照文档构建。

## 代码期望（概览）

| 区域 | 说明 |
|------|------|
| **Kotlin / Compose**（`trierarch-app/`） | 遵循常见 Android/Kotlin 写法，保持 UI 逻辑可读。 |
| **Rust**（`trierarch-native/`） | 提交前尽量 `cargo fmt`、`cargo clippy`。 |
| **C / NDK**（`trierarch-proot/`、`trierarch-wayland/`） | 与现有风格一致；逻辑修复与大规模格式化尽量分开提交。 |

## 文档

- 面向用户的行为变化：必要时更新根目录 [`README.zh.md`](README.zh.md) / [`README.md`](README.md)。
- 构建说明：[`README_DEV.zh.md`](README_DEV.zh.md) 与各子目录 **模块 README**。
- 发版：[`docs/RELEASING.zh.md`](docs/RELEASING.zh.md)（[English](docs/RELEASING.md)）。
- Plasma/rootfs 调优：[`trierarch-optimize/`](trierarch-optimize/)。

## 许可

参与贡献即表示你同意以与本项目相同的许可条款授权你的贡献（见根目录 [`LICENSE`](LICENSE)）。

## 有疑问

若有不清楚之处，可开 **Issue**，标题或正文标明是**提问**——也方便后来的人检索。
