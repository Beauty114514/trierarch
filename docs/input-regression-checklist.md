# Input Regression Checklist

English | 中文

Use this checklist before release, and after any input-related change.

## 0) Test Setup

- Device: at least 1 wired keyboard and 1 Bluetooth keyboard
- Mode: Wayland desktop visible (`Display` on)
- Input methods: hardware keyboard + soft keyboard + touch pointer mode
- Clean state: restart app once before test round

## 1) Soft Keyboard (IME)

- [ ] Open soft keyboard from menu
- [ ] Type ASCII letters/digits/symbols
- [ ] Type Enter / Tab
- [ ] Input non-ASCII (e.g. Chinese / emoji)
- [ ] Long paste (>= 200 chars) does not freeze UI

## 2) Hardware Keyboard Basics

- [ ] A-Z / 0-9 / common symbols
- [ ] Enter / Backspace / Tab / Esc
- [ ] Arrow keys / Home / End / Insert / Delete
- [ ] F1-F12 (and F13-F24 if available)

## 3) Modifier Keys Stability

- [ ] Shift press/release (left and right) returns to normal state
- [ ] Ctrl press/release (left and right) returns to normal state
- [ ] Alt press/release (left and right) returns to normal state
- [ ] Meta press/release (left and right) returns to normal state
- [ ] Fast repeat press does not cause sticky modifier state

## 4) Lock Keys + Fn

- [ ] CapsLock toggles predictably
- [ ] NumLock toggles keypad behavior predictably
- [ ] ScrollLock toggles without corrupting other key states
- [ ] Fn and Fn-combos (if keyboard exposes them) do not break modifiers
- [ ] Alt+CapsLock triggers in-desktop window switch behavior

## 5) Shortcut Sanity

- [ ] Ctrl+C / Ctrl+V / Ctrl+X in terminal or editor
- [ ] Ctrl+L / Ctrl+A
- [ ] Alt+Tab behavior matches current design expectations
- [ ] App remains inside Trierarch desktop during supported shortcuts

## 6) Pointer + Keyboard Mixed Use

- [ ] Hardware mouse move/click/scroll still works after keyboard stress test
- [ ] Touchpad mode gestures still work after keyboard stress test
- [ ] No input lag spike after repeated key combinations

## 7) Pass/Fail Rule

- If any key gets stuck or state desync appears: **FAIL**
- If behavior differs between wired and Bluetooth keyboard: **FAIL**
- If only one keyboard model fails: keep logs and file issue with model/vendor

---

# 输入回归清单

每次发版前、以及任何输入相关改动后，都按这份清单回归。

## 0）测试准备

- 设备：至少 1 把有线键盘 + 1 把蓝牙键盘
- 场景：进入 Wayland 桌面（`Display` 打开）
- 输入方式：硬键盘 + 软键盘 + 触摸指针模式
- 干净状态：每轮测试前重启一次 App

## 1）软键盘（IME）

- [ ] 菜单呼出软键盘
- [ ] 输入英文/数字/符号
- [ ] 输入 Enter / Tab
- [ ] 输入非 ASCII（中文/emoji）
- [ ] 长粘贴（>=200 字符）不导致 UI 卡死

## 2）硬键盘基础

- [ ] A-Z / 0-9 / 常用符号
- [ ] Enter / Backspace / Tab / Esc
- [ ] 方向键 / Home / End / Insert / Delete
- [ ] F1-F12（有条件可测 F13-F24）

## 3）修饰键稳定性

- [ ] Shift（左右）按下/释放后状态恢复正常
- [ ] Ctrl（左右）按下/释放后状态恢复正常
- [ ] Alt（左右）按下/释放后状态恢复正常
- [ ] Meta（左右）按下/释放后状态恢复正常
- [ ] 快速连按不会出现“粘住”

## 4）锁定键 + Fn

- [ ] CapsLock 切换行为稳定
- [ ] NumLock 能正确切换小键盘行为
- [ ] ScrollLock 切换后不影响其他键状态
- [ ] Fn 及 Fn 组合键不会破坏修饰键状态
- [ ] Alt+CapsLock 能触发桌面内窗口切换

## 5）快捷键健全性

- [ ] Ctrl+C / Ctrl+V / Ctrl+X
- [ ] Ctrl+L / Ctrl+A
- [ ] Alt+Tab 行为符合当前设计预期
- [ ] 支持的快捷键不会把应用切出 Trierarch

## 6）鼠标与键盘混合场景

- [ ] 键盘压测后硬件鼠标移动/点击/滚轮正常
- [ ] 键盘压测后触摸板模式手势正常
- [ ] 组合键高频触发后无明显输入延迟突增

## 7）判定标准

- 出现任意按键滞粘或状态错乱：**失败**
- 有线/蓝牙表现不一致：**失败**
- 仅某型号键盘异常：保留日志并记录型号后提 issue
