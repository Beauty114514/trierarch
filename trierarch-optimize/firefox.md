# Firefox (Trierarch / proot)

## Why it breaks

1. **Sandboxing & namespaces** — Firefox’s multiprocess model and **content-process sandbox** assume Linux namespaces, `/proc`, permissions, etc. Under **proot**’s user namespace and filesystem view, the default sandbox profile often **does not match** what Firefox expects, which can **crash at startup** or behave badly.  
2. **Out of the box** — Leaving sandbox settings at desktop defaults is unsafe for this environment.

## What to do

### 1. Install Firefox

Inside proot:

```bash
pacman -S firefox
```

If you need the keyring first:

```bash
pacman-key --init
pacman-key --populate archlinux
pacman -S firefox
```

### 2. Change about:config (required)

1. Open **`about:config`** → accept the risk prompt.  
2. Search **`sandbox`** and set:  
   - **`media.cubed.sandbox`** → **`false`**  
   - **`security.sandbox.content.level`** → **`0`**  
3. **Restart** Firefox.

| Setting | New value |
|---------|-----------|
| `media.cubed.sandbox` | `false` |
| `security.sandbox.content.level` | `0` |

## Why this works

- Lowering the **content sandbox** stops Firefox from relying on kernel/sandbox primitives that are **often incomplete or inconsistent under proot**, so the browser can run in a plain user-namespace stack.  
- Trade-off: **weaker sandboxing**—acceptable for many local proot desktop sessions; don’t reuse the same profile for untrusted browsing without extra isolation.
