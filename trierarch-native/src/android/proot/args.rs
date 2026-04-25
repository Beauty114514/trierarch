//! Proot argv and environment for the interactive shell.
//!
//! With a prepared rootfs: bind `/dev`, `/proc`, `/sys`, Wayland and Pulse runtime dirs, shim files
//! from extraction, then `bash -i`. Without rootfs: `/system/bin/sh` for a minimal shell.

use super::super::application_context::{get_application_context, has_rootfs};
use super::super::pulse_host;
use anyhow::{Context, Result};
use std::ffi::CString;
use std::fs::File;
use std::path::Path;

const PULSE_CLIENT_NO_SHM: &str = "\
# trierarch: bind-mounted; do not edit.\n\
# Host Pulse uses socket IPC only (no memfd across namespaces).\n\
enable-shm = no\n\
enable-memfd = no\n\
";

const GUEST_PROFILE_TRIERARCH_RUNTIME: &str = "\
# trierarch: bind-mounted; do not edit.\n\
# Keep essential runtime defaults across `su - user` (login shells read /etc/profile.d).\n\
# Only sets defaults when variables are unset; user exports remain authoritative.\n\
\n\
: \"${XDG_RUNTIME_DIR:=/run/user/0}\"\n\
: \"${PULSE_SERVER:=unix:/run/trierarch-pulse/native}\"\n\
export XDG_RUNTIME_DIR PULSE_SERVER\n\
\n\
# Best-effort: set default sink if server becomes reachable.\n\
if command -v pactl >/dev/null 2>&1; then\n\
  for _i in 1 2 3 4 5 6 7 8 9 10; do\n\
    pactl info >/dev/null 2>&1 && break\n\
    sleep 0.1\n\
  done\n\
  pactl set-default-sink trierarch-out >/dev/null 2>&1 || true\n\
fi\n\
";

fn write_pulse_guest_client_fragment(data_dir: &std::path::Path) -> std::path::PathBuf {
    let path = data_dir.join("proot_pulse_client_no_shm.conf");
    if let Err(e) = std::fs::write(path.as_path(), PULSE_CLIENT_NO_SHM) {
        log::warn!("proot: write {:?}: {:?}", path, e);
    }
    path
}

fn write_guest_profile_fragment(data_dir: &std::path::Path) -> std::path::PathBuf {
    let path = data_dir.join("proot_profile_trierarch_runtime.sh");
    if let Err(e) = std::fs::write(path.as_path(), GUEST_PROFILE_TRIERARCH_RUNTIME) {
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

        // GPU device nodes (Turnip/Freedreno): some Android+PRoot environments can't transparently
        // expose the full host /dev tree. Bind the critical nodes explicitly when present.
        //
        // - Adreno KGSL: /dev/kgsl-3d0
        // - DRM render nodes: /dev/dri/renderD*
        //
        // These binds are safe even when the guest can't list /dev entries; direct open() is enough.
        if Path::new("/dev/kgsl-3d0").exists() {
            argv.push(CString::new("--bind=/dev/kgsl-3d0:/dev/kgsl-3d0").unwrap());
        }
        // NOTE: On many Android builds, `/dev/dri` exists but is blocked by SELinux for app UIDs.
        // Binding it into the guest makes Mesa/Turnip probe fail with EACCES early. Only expose it
        // when the current process can actually open it.
        if Path::new("/dev/dri").exists() && File::open("/dev/dri").is_ok() {
            argv.push(CString::new("--bind=/dev/dri:/dev/dri").unwrap());
        } else {
            log::info!("proot: skip bind /dev/dri (missing or not accessible)");
        }

        let wayland_runtime = ctx.data_dir.join("usr").join("tmp");
        let _ = std::fs::create_dir_all(&wayland_runtime);
        // X11: bind the host (app process) Unix socket directory into the guest /tmp/.X11-unix so
        // DISPLAY=:0 works inside proot.
        let host_x11_dir = ctx.data_dir.join("tmp").join(".X11-unix");
        let _ = std::fs::create_dir_all(&host_x11_dir);
        // Ensure the guest-side mountpoint exists inside the rootfs; proot bind targets must exist.
        let guest_x11_dir = rootfs.join("tmp/.X11-unix");
        let _ = std::fs::create_dir_all(&guest_x11_dir);
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let _ = std::fs::set_permissions(&host_x11_dir, std::fs::Permissions::from_mode(0o1777));
            let _ = std::fs::set_permissions(&guest_x11_dir, std::fs::Permissions::from_mode(0o1777));
        }
        argv.push(
            CString::new(format!("--bind={}:/run/user/0", wayland_runtime.display())).unwrap(),
        );
        argv.push(
            CString::new(format!(
                "--bind={}:{}",
                host_x11_dir.display(),
                "/tmp/.X11-unix"
            ))
            .context("x11 unix socket bind")?,
        );
        // Always bind the vtest socket directory. This keeps the proot environment stable and lets
        // the UI's startup snippet opt into VIRGL/VENUS without needing any global "mode" state.
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

        // Ensure PULSE_SERVER/XDG_RUNTIME_DIR survive `su - user` by providing a login-shell profile fragment.
        let profile_frag = write_guest_profile_fragment(&ctx.data_dir);
        argv.push(
            CString::new(format!(
                "--bind={}:/etc/profile.d/99-trierarch-runtime.sh",
                profile_frag.display()
            ))
            .context("profile.d runtime bind")?,
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
    env.push(CString::new("QT_QUICK_BACKEND=software").unwrap());

    // IMPORTANT: do not force graphics driver env vars here.
    //
    // Users may intentionally export their own Vulkan/OpenGL stack (e.g. Turnip + Zink) inside the
    // proot session. Injecting `GALLIUM_DRIVER=llvmpipe` or `VK_ICD_FILENAMES=virtio_icd.json` here
    // would override those choices and make troubleshooting impossible.
    //
    // Instead, we export only the mode labels below (`TRIERARCH_*_MODE`) and let the UI-injected
    // startup snippet set defaults **only if unset** for newly spawned sessions.
    // Provide stable VIRGL/VENUS-related defaults without selecting any driver.
    // These do not clobber user-provided graphics stacks (Turnip/Zink/etc).
    env.push(CString::new("VTEST_SOCKET_NAME=/run/trierarch-virgl/vtest.sock").unwrap());
    env.push(CString::new("VTEST_RENDERER_SOCKET_NAME=/run/trierarch-virgl/vtest.sock").unwrap());
    if has_rootfs(rootfs) {
        env.insert(3, CString::new("PS1=[\\u@\\h \\W]\\$ ").unwrap());
        env.push(
            CString::new(format!(
                "PULSE_SERVER={}",
                pulse_host::guest_pulse_server_env()
            ))
            .context("PULSE_SERVER")?,
        );
        // Xwayland: no glamor (no DRM GBM device in this compositor path).
        env.push(CString::new("XWAYLAND_NO_GLAMOR=1").unwrap());
    }

    Ok((argv, env))
}
