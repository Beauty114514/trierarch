//! Proot argv and environment for the interactive shell.
//!
//! With a prepared rootfs: bind `/dev`, `/proc`, `/sys`, Wayland and Pulse runtime dirs, shim files
//! from extraction, then `bash -i`. Without rootfs: `/system/bin/sh` for a minimal shell.

use super::super::application_context::{
    get_application_context, get_renderer_mode, has_rootfs, RendererMode,
};
use super::super::pulse_host;
use anyhow::{Context, Result};
use std::ffi::CString;

const PULSE_CLIENT_NO_SHM: &str = "\
# trierarch: bind-mounted; do not edit.\n\
# Host Pulse uses socket IPC only (no memfd across namespaces).\n\
enable-shm = no\n\
enable-memfd = no\n\
";

fn write_pulse_guest_client_fragment(data_dir: &std::path::Path) -> std::path::PathBuf {
    let path = data_dir.join("proot_pulse_client_no_shm.conf");
    if let Err(e) = std::fs::write(path.as_path(), PULSE_CLIENT_NO_SHM) {
        log::warn!("proot: write {:?}: {:?}", path, e);
    }
    path
}

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
    let renderer_mode = get_renderer_mode().unwrap_or(RendererMode::LlvmPipe);

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
        argv.push(
            CString::new(format!("--bind={}:/run/user/0", wayland_runtime.display())).unwrap(),
        );
        // UNIVERSAL only: bind vtest socket dir (LLVMPIPE must not see a stale socket).
        if matches!(renderer_mode, RendererMode::Universal) {
            let virgl_runtime = ctx.data_dir.join("virgl-run");
            let _ = std::fs::create_dir_all(&virgl_runtime);
            argv.push(
                CString::new(format!(
                    "--bind={}:{}",
                    virgl_runtime.display(),
                    "/run/trierarch-virgl"
                ))
                .context("virgl runtime bind")?,
            );
        }
        let pulse_rt = pulse_host::host_pulse_runtime_dir(&ctx.data_dir);
        let _ = std::fs::create_dir_all(&pulse_rt);
        argv.push(
            CString::new(format!(
                "--bind={}:{}",
                pulse_rt.display(),
                pulse_host::GUEST_PULSE_RUNTIME_MOUNT
            ))
            .context("pulse runtime bind")?,
        );
        let pulse_client_frag = write_pulse_guest_client_fragment(&ctx.data_dir);
        argv.push(
            CString::new(format!(
                "--bind={}:/etc/pulse/client.conf.d/99-trierarch-noshm.conf",
                pulse_client_frag.display()
            ))
            .context("pulse client no-shm bind")?,
        );
        argv.push(CString::new(format!("--bind={}/tmp:/dev/shm", rootfs.display())).unwrap());
        if let Some(ref sdcard) = ctx.external_storage_path {
            if sdcard.exists() {
                argv.push(CString::new(format!("--bind={}:/android", sdcard.display())).unwrap());
                argv.push(
                    CString::new(format!("--bind={}:/root/Android", sdcard.display())).unwrap(),
                );
            }
        }
        argv.push(CString::new("--bind=/dev/urandom:/dev/random").unwrap());
        argv.push(CString::new("--bind=/proc/self/fd:/dev/fd").unwrap());
        argv.push(CString::new("--bind=/proc/self/fd/0:/dev/stdin").unwrap());
        argv.push(CString::new("--bind=/proc/self/fd/1:/dev/stdout").unwrap());
        argv.push(CString::new("--bind=/proc/self/fd/2:/dev/stderr").unwrap());
        argv.push(
            CString::new(format!(
                "--bind={}/proc/.loadavg:/proc/loadavg",
                rootfs.display()
            ))
            .unwrap(),
        );
        argv.push(
            CString::new(format!("--bind={}/proc/.stat:/proc/stat", rootfs.display())).unwrap(),
        );
        argv.push(
            CString::new(format!(
                "--bind={}/proc/.uptime:/proc/uptime",
                rootfs.display()
            ))
            .unwrap(),
        );
        argv.push(
            CString::new(format!(
                "--bind={}/proc/.version:/proc/version",
                rootfs.display()
            ))
            .unwrap(),
        );
        argv.push(
            CString::new(format!(
                "--bind={}/proc/.vmstat:/proc/vmstat",
                rootfs.display()
            ))
            .unwrap(),
        );
        argv.push(
            CString::new(format!(
                "--bind={}/proc/.sysctl_entry_cap_last_cap:/proc/sys/kernel/cap_last_cap",
                rootfs.display()
            ))
            .unwrap(),
        );
        argv.push(CString::new(format!("--bind={}/proc/.sysctl_inotify_max_user_watches:/proc/sys/fs/inotify/max_user_watches", rootfs.display())).unwrap());
        argv.push(
            CString::new(format!(
                "--bind={}/sys/.empty:/sys/fs/selinux",
                rootfs.display()
            ))
            .unwrap(),
        );
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
        CString::new("QT_QPA_PLATFORM=wayland").unwrap(),
        CString::new("USER=root").unwrap(),
        CString::new("LOGNAME=root").unwrap(),
    ];
    match renderer_mode {
        RendererMode::Universal => {
            env.push(CString::new("QT_QUICK_BACKEND=software").unwrap());
            env.push(CString::new("GALLIUM_DRIVER=virpipe").unwrap());
            env.push(CString::new("MESA_LOADER_DRIVER_OVERRIDE=virpipe").unwrap());
            env.push(CString::new("VTEST_SOCKET_NAME=/run/trierarch-virgl/vtest.sock").unwrap());
            env.push(
                CString::new("VTEST_RENDERER_SOCKET_NAME=/run/trierarch-virgl/vtest.sock").unwrap(),
            );
            env.push(CString::new("LIBGL_ALWAYS_SOFTWARE=0").unwrap());
            env.push(
                CString::new("VK_DRIVER_FILES=/usr/share/vulkan/icd.d/virtio_icd.json").unwrap(),
            );
            env.push(CString::new("VN_DEBUG=vtest").unwrap());
        }
        RendererMode::LlvmPipe => {
            env.push(CString::new("QT_QUICK_BACKEND=software").unwrap());
            env.push(CString::new("LIBGL_ALWAYS_SOFTWARE=1").unwrap());
            env.push(CString::new("GALLIUM_DRIVER=llvmpipe").unwrap());
            env.push(CString::new("MESA_LOADER_DRIVER_OVERRIDE=llvmpipe").unwrap());
        }
    }
    if has_rootfs(rootfs) {
        env.insert(3, CString::new("PS1=[\\u@\\h \\W]\\$ ").unwrap());
        env.push(
            CString::new(format!(
                "PULSE_SERVER={}",
                pulse_host::guest_pulse_server_env()
            ))
            .context("PULSE_SERVER")?,
        );
        env.push(
            CString::new(format!(
                "TRIERARCH_RENDERER_MODE={}",
                renderer_mode.as_env_value()
            ))
            .context("TRIERARCH_RENDERER_MODE")?,
        );
        // Xwayland: no glamor (no DRM GBM device in this compositor path).
        env.push(CString::new("XWAYLAND_NO_GLAMOR=1").unwrap());
    }

    Ok((argv, env))
}
