//! Build the proot argv/env used to spawn the interactive shell.
//!
//! Contract:
//! - If the rootfs is ready, we enter it with a set of binds to provide `/dev`, `/proc`, `/sys`,
//!   Wayland runtime socket directory, and a few compatibility shims.
//! - If the rootfs is not ready, we fall back to Android's `/system/bin/sh` for diagnostics.
//! - Environment variables are chosen to make common desktop stacks work under proot + Wayland
//!   (toolkits, runtime dir, and software rendering defaults).
//!
//! Notes:
//! - Many binds map "fake" proc files created during rootfs extraction; this keeps proot happy
//!   on Android where proc/sys are not fully writable or namespaced.

use super::super::application_context::{get_application_context, has_rootfs};
use anyhow::{Context, Result};
use std::ffi::CString;

fn proot_and_loader_paths() -> Result<(std::path::PathBuf, std::path::PathBuf)> {
    let ctx = get_application_context()?;
    let proot = ctx.native_library_dir.join("libproot.so");
    let loader = ctx.native_library_dir.join("libproot_loader.so");
    if !proot.exists() {
        anyhow::bail!("proot not found: {:?}", proot);
    }
    if !loader.exists() {
        anyhow::bail!("loader not found: {:?}", loader);
    }
    Ok((proot, loader))
}

pub(super) fn build_exec_args(rootfs: &std::path::Path) -> Result<(Vec<CString>, Vec<CString>)> {
    let ctx = get_application_context()?;
    let (proot, loader) = proot_and_loader_paths()?;

    let proot_str = proot.to_string_lossy();
    let loader_str = loader.to_string_lossy();

    let mut argv: Vec<CString> = vec![CString::new(proot_str.as_bytes()).context("proot path")?];

    if has_rootfs(rootfs) {
        argv.push(CString::new("-r").unwrap());
        argv.push(CString::new(rootfs.to_string_lossy().as_bytes()).context("rootfs path")?);
        argv.push(CString::new("-L").unwrap());
        argv.push(CString::new("--link2symlink").unwrap());
        argv.push(CString::new("--sysvipc").unwrap());
        argv.push(CString::new("--kill-on-exit").unwrap());
        argv.push(CString::new("--root-id").unwrap());
        argv.push(CString::new("--bind=/dev").unwrap());
        argv.push(CString::new("--bind=/proc").unwrap());
        argv.push(CString::new("--bind=/sys").unwrap());
        let wayland_runtime = ctx.data_dir.join("usr").join("tmp");
        let _ = std::fs::create_dir_all(&wayland_runtime);
        let x11_dir = rootfs.join("tmp/.X11-unix");
        let _ = std::fs::create_dir_all(&x11_dir);
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let _ = std::fs::set_permissions(&x11_dir, std::fs::Permissions::from_mode(0o1777));
        }
        argv.push(CString::new(format!("--bind={}:/run/user/0", wayland_runtime.display())).unwrap());
        argv.push(CString::new(format!("--bind={}/tmp:/dev/shm", rootfs.display())).unwrap());
        if let Some(ref sdcard) = ctx.external_storage_path {
            if sdcard.exists() {
                argv.push(CString::new(format!("--bind={}:/android", sdcard.display())).unwrap());
                argv.push(CString::new(format!("--bind={}:/root/Android", sdcard.display())).unwrap());
            }
        }
        argv.push(CString::new("--bind=/dev/urandom:/dev/random").unwrap());
        argv.push(CString::new("--bind=/proc/self/fd:/dev/fd").unwrap());
        argv.push(CString::new("--bind=/proc/self/fd/0:/dev/stdin").unwrap());
        argv.push(CString::new("--bind=/proc/self/fd/1:/dev/stdout").unwrap());
        argv.push(CString::new("--bind=/proc/self/fd/2:/dev/stderr").unwrap());
        argv.push(CString::new(format!("--bind={}/proc/.loadavg:/proc/loadavg", rootfs.display())).unwrap());
        argv.push(CString::new(format!("--bind={}/proc/.stat:/proc/stat", rootfs.display())).unwrap());
        argv.push(CString::new(format!("--bind={}/proc/.uptime:/proc/uptime", rootfs.display())).unwrap());
        argv.push(CString::new(format!("--bind={}/proc/.version:/proc/version", rootfs.display())).unwrap());
        argv.push(CString::new(format!("--bind={}/proc/.vmstat:/proc/vmstat", rootfs.display())).unwrap());
        argv.push(CString::new(format!("--bind={}/proc/.sysctl_entry_cap_last_cap:/proc/sys/kernel/cap_last_cap", rootfs.display())).unwrap());
        argv.push(CString::new(format!("--bind={}/proc/.sysctl_inotify_max_user_watches:/proc/sys/fs/inotify/max_user_watches", rootfs.display())).unwrap());
        argv.push(CString::new(format!("--bind={}/sys/.empty:/sys/fs/selinux", rootfs.display())).unwrap());
        argv.push(CString::new("/bin/bash").unwrap());
        argv.push(CString::new("-i").unwrap());
    } else {
        argv.push(CString::new("-0").unwrap());
        argv.push(CString::new("/system/bin/sh").unwrap());
        argv.push(CString::new("-c").unwrap());
        argv.push(CString::new("export PS1='[root@android \\W]# '; exec sh -i").unwrap());
    }

    let mut env: Vec<CString> = vec![
        CString::new(format!("PROOT_LOADER={}", loader_str)).unwrap(),
        CString::new(format!("PROOT_TMP_DIR={}", ctx.cache_dir.display())).unwrap(),
        CString::new("HOME=/root").unwrap(),
        CString::new("TERM=xterm-256color").unwrap(),
        CString::new("LANG=C.UTF-8").unwrap(),
        CString::new("PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/games:/usr/games:/system/bin:/system/xbin").unwrap(),
        CString::new("TMPDIR=/tmp").unwrap(),
        CString::new("XDG_RUNTIME_DIR=/run/user/0").unwrap(),
        CString::new("WAYLAND_DISPLAY=wayland-trierarch").unwrap(),
        CString::new("XDG_SESSION_TYPE=wayland").unwrap(),
        CString::new("LIBGL_ALWAYS_SOFTWARE=1").unwrap(),
        CString::new("GALLIUM_DRIVER=llvmpipe").unwrap(),
        CString::new("MESA_LOADER_DRIVER_OVERRIDE=llvmpipe").unwrap(),
        CString::new("QT_QUICK_BACKEND=software").unwrap(),
        CString::new("QT_QPA_PLATFORM=wayland").unwrap(),
        CString::new("USER=root").unwrap(),
        CString::new("LOGNAME=root").unwrap(),
    ];
    if has_rootfs(rootfs) {
        env.insert(3, CString::new("PS1=[\\u@\\h \\W]\\$ ").unwrap());
    }

    Ok((argv, env))
}
