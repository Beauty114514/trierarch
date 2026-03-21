# Firefox（Trierarch / proot）

## 原因

1. **沙箱与命名空间**：Firefox 的多进程与 **内容进程沙箱**依赖 Linux 的命名空间、`/proc`、权限等；在 **proot** 提供的用户命名空间与文件系统视图下，默认沙箱路径与假设与常规桌面不一致，易导致 **启动即崩溃或未定义行为**。  
2. **未改配置时**：默认开启的沙箱等级会在本环境中触发不兼容。

## 解决方法

### 1. 安装 Firefox

在 proot 内：

```bash
pacman -S firefox
```

若需初始化密钥环：

```bash
pacman-key --init
pacman-key --populate archlinux
pacman -S firefox
```

### 2. 调整 about:config（关键）

1. 地址栏打开：`about:config` → 接受风险。  
2. 搜索 **`sandbox`**，修改：  
   - **`media.cubed.sandbox`** → **`false`**  
   - **`security.sandbox.content.level`** → **`0`**  
3. **重启** Firefox。

| 配置项 | 修改为 |
|--------|--------|
| `media.cubed.sandbox` | `false` |
| `security.sandbox.content.level` | `0` |

## 为什么能解决

- 降低或关闭 **内容沙箱等级**后，Firefox 不再依赖那些在 proot 下常失败或行为不一致的内核/命名空间能力，从而能在 **用户态容器**里稳定运行。  
- 代价是沙箱防护减弱；在 **本机 proot 桌面会话**内通常可接受，请勿与不可信网页/插件混用同一配置而不加额外隔离。
