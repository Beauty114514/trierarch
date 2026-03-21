# GTK 类应用中的中文等输入（字体 + fcitx5）

> 与根目录 [`README.zh.md`](../README.zh.md) 一致：**GTK** 应用对 `Ctrl+Shift+U` 的 Unicode 输入支持较好；**Qt** 应用在同样环境下可能不完整，需用 **GTK 应用（如 Mousepad）中转复制粘贴**，见用户文档。

## 原因

1. **缺字体**：未安装中日韩等字体时，界面容易出现「豆腐块」或无法正常选字显示。  
2. **输入法与会话**：在 Plasma（Wayland）下若未安装/未启动输入法框架（如 **fcitx5**），部分应用内无法形成完整的「按键 → Unicode」链路；而 Trierarch 侧已通过软键盘向 Linux 侧注入 `Ctrl+Shift+U` 序列，**GTK** 侧通常能正确消费该序列完成输入。  
3. **Qt 限制**：属客户端与协议栈差异，**不能**仅靠本教程在 Qt 内完全对齐 GTK 的体验（见上「中转」说明）。

## 解决方法

### 最小步骤（字体 + 启动 fcitx5）

在 **proot 内 Arch** 终端（按需 `sudo`）：

```bash
sudo pacman -S --needed noto-fonts-cjk wqy-microhei
```

会话中启动 fcitx5（也可在 **系统设置 → 自动启动** 里添加）：

```bash
fcitx5 &
```

### 更完整的一套（推荐在 Plasma 里长期使用）

```bash
sudo pacman -S --needed noto-fonts-cjk noto-fonts fcitx5 fcitx5-chinese-addons fcitx5-configtool
```

在 **系统设置 → 键盘 / 区域设置 / 输入法** 中把输入法框架设为 **Fcitx5**，按向导添加拼音等；装好后可在 **系统设置 → 字体** 中确认默认字体，必要时注销再进会话。

之后在 **GTK 类应用**（如 Firefox、Mousepad）中：  
- 使用 **fcitx5** 的切换键做拼音等输入；和/或  
- 使用 **`Ctrl+Shift+U`** 按根目录文档做 Unicode 直接输入（与 Trierarch 注入方式配合）。

### Qt 应用

若 Qt 下仍异常，按根目录 README：**在 GTK 里输入 → 复制 → 粘贴到 Qt**。

## 为什么能解决

- **字体包**提供字形数据，避免显示层缺字。  
- **fcitx5** 在 GTK/Qt 中提供标准输入法服务；与 Trierarch 的 `Ctrl+Shift+U` 注入叠加后，**GTK** 路径最稳定。  
- **Qt** 若仍不完整，用 **GTK 中转** 规避，属已知限制。
