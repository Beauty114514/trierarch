# Firefox（Trierarch / proot）

在 Trierarch 的 Arch 环境中安装并运行 Firefox。不做以下配置修改时，Firefox 可能会崩溃。

## 1. 安装 Firefox

在 Trierarch 终端（proot shell）中执行：

```bash
pacman -S firefox
```

提示时输入 `Y` 确认。若需先初始化密钥环：

```bash
pacman-key --init
pacman-key --populate archlinux
```

再执行一次 `pacman -S firefox` 即可。

## 2. 避免崩溃（about:config）

在此环境中，Firefox 可能因沙箱相关设置而崩溃，需在 about:config 中修改以下两项。

1. **打开配置页**
   - 在 Firefox 地址栏输入：`about:config`
   - 按 **回车**。

2. **接受警告**
   - 在弹出界面点击 **「接受风险并继续」**，进入 preferences（about:config）设置页。

3. **搜索并修改**
   - 在 about:config 页面的 **搜索框** 中输入：`sandbox`
   - 找到并修改：
     - **`media.cubed.sandbox`** → 改为 **`false`**
     - **`security.sandbox.content.level`** → 改为 **`0`**
   - （双击该条目的值即可修改，或使用右侧编辑控件。）

4. **重启**
   - 重启 Firefox 后即可正常使用。

## 小结

| 配置项                           | 修改为   |
|----------------------------------|----------|
| `media.cubed.sandbox`            | `false`  |
| `security.sandbox.content.level` | `0`      |
