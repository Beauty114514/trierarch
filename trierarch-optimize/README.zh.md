# trierarch-optimize

[English](README.md) | 中文

---

本目录收录 **因 Trierarch 自身栈带来的体验问题** 的说明与对策，主要包括：

- 本应用**自研 Wayland 合成器**与输入路径（例如 `Ctrl+Shift+U` 注入）和 **GTK/Qt** 的差异；  
- **proot**（及与 Termux 生态同类的用户态容器）在命名空间、文件系统、`/proc`、**inotify** 等与真机不同导致的限制。

**若问题属于 Arch Linux、KDE、某软件在普通 x86 桌面上的通用配置**，请优先查阅 **[ArchWiki](https://wiki.archlinux.org/)** 与上游文档；这里只补充「在 Trierarch + proot + 本合成器」组合下**额外容易遇到**的一类事。

## 教程（模板：原因 → 做法 → 为何有效）

| 主题 | 文档 |
|------|------|
| Firefox 在 proot 中崩溃 | [`firefox.zh.md`](firefox.zh.md) |
| GTK 类应用中文等输入（字体 + fcitx5） | [`input-cjk-gtk.zh.md`](input-cjk-gtk.zh.md) |
| KDE 中 Baloo / `tags:/` 相关骚扰提示 | [`baloo-tags-warning.zh.md`](baloo-tags-warning.zh.md) |

（后续若有新篇，请在此表补充并沿用同一模板。）
