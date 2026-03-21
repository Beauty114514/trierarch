// Spawn proot from this process; parent of proot is native.
// Uses forkpty for Termux-like terminal (controlling tty, job control, colors).

use super::application_context::{get_application_context, has_rootfs, rootfs_dir};
use anyhow::{Context, Result};
use nix::pty::{forkpty, ForkptyResult, Winsize};
use nix::sys::signal::Signal;
use nix::unistd::{dup, execve, Pid};
use std::collections::VecDeque;
use std::ffi::CString;
use std::fs::File;
use std::io::{Read, Write};
use std::os::fd::IntoRawFd;
use std::os::unix::io::FromRawFd;
use std::sync::Arc;

/// Wrapper for child process spawned via forkpty (replaces std::process::Child).
pub struct ChildProcess {
    pub pid: Pid,
}

impl Drop for ChildProcess {
    fn drop(&mut self) {
        let _ = nix::sys::signal::kill(self.pid, Signal::SIGTERM);
    }
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

/// Build argv and env for execve. Must be done before fork (no alloc in child).
fn build_exec_args(rootfs: &std::path::Path) -> Result<(Vec<CString>, Vec<CString>)> {
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
        // Wayland: app creates socket in data_dir/usr/tmp; bind to /run/user/0 so clients in proot can connect.
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
        argv.push(CString::new("-c").unwrap());
        argv.push(CString::new("echo; export PS1='[\\u@\\h \\W]\\$ '; exec /bin/bash -i").unwrap());
    } else {
        argv.push(CString::new("-0").unwrap());
        argv.push(CString::new("/system/bin/sh").unwrap());
        argv.push(CString::new("-c").unwrap());
        argv.push(CString::new("export PS1='[root@android \\W]# '; exec sh -i").unwrap());
    }

    let env: Vec<CString> = vec![
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

    Ok((argv, env))
}

/// ANSI escape sequence state for stripping control sequences from PTY output.
/// CSI = ESC [ ... (params) final_byte. OSC = ESC ] ... BEL or ESC \.
#[derive(Clone, Copy)]
enum AnsiState {
    Normal,
    /// Saw ESC, next byte determines sequence type.
    Escape,
    /// CSI: ESC [ ... consuming until final byte (0x40..=0x7e).
    Csi,
    /// OSC: ESC ] ... consuming until BEL (0x07) or ESC \.
    Osc,
    /// Saw ESC inside OSC, expecting \ to end.
    OscEsc,
}

/// Read bytes from PTY master, split by newlines, push to lines.
/// Strips ANSI escape sequences (colors, cursor movement, OSC title, etc.) so output is plain text.
/// Also updates partial_line (prompt without trailing newline) so it can be displayed.
const MAX_LINES: usize = 5000;

fn read_pty_to_lines(
    mut master: File,
    lines: Arc<std::sync::Mutex<VecDeque<String>>>,
    partial_line: Arc<std::sync::Mutex<String>>,
) {
    let mut buf = [0u8; 4096];
    let mut line = String::new();
    let mut ansi = AnsiState::Normal;
    loop {
        match master.read(&mut buf) {
            Ok(0) => break,
            Ok(n) => {
                for &b in &buf[..n] {
                    match ansi {
                        AnsiState::Escape => {
                            ansi = match b {
                                b'[' => AnsiState::Csi,
                                b']' => AnsiState::Osc,
                                _ if (0x40..=0x7e).contains(&b) => AnsiState::Normal,
                                _ => AnsiState::Escape,
                            };
                        }
                        AnsiState::Csi => {
                            if (0x40..=0x7e).contains(&b) {
                                ansi = AnsiState::Normal;
                            }
                        }
                        AnsiState::Osc => {
                            if b == 0x07 {
                                ansi = AnsiState::Normal;
                            } else if b == 0x1b {
                                ansi = AnsiState::OscEsc;
                            }
                        }
                        AnsiState::OscEsc => {
                            ansi = if b == b'\\' {
                                AnsiState::Normal
                            } else {
                                AnsiState::Osc
                            };
                        }
                        AnsiState::Normal => match b {
                            b'\n' => {
                                let l = std::mem::take(&mut line);
                                if !l.is_empty() {
                                    if let Ok(mut v) = lines.lock() {
                                        v.push_back(l);
                                        while v.len() > MAX_LINES {
                                            v.pop_front();
                                        }
                                    }
                                }
                                let _ = partial_line.lock().map(|mut p| *p = String::new());
                            }
                            b'\r' => {
                                let l = std::mem::take(&mut line);
                                if !l.is_empty() {
                                    if let Ok(mut v) = lines.lock() {
                                        v.push_back(l);
                                        while v.len() > MAX_LINES {
                                            v.pop_front();
                                        }
                                    }
                                }
                                let _ = partial_line.lock().map(|mut p| *p = String::new());
                            }
                            0x1b => ansi = AnsiState::Escape,
                            _ => {
                                if b >= 0x20 || b == b'\t' {
                                    line.push(b as char);
                                    let _ = partial_line.lock().map(|mut p| *p = line.clone());
                                }
                            }
                        },
                    }
                }
            }
            Err(_) => break,
        }
    }
    let l = std::mem::take(&mut line);
    if !l.is_empty() {
        if let Ok(mut v) = lines.lock() {
            v.push_back(l);
            while v.len() > MAX_LINES {
                v.pop_front();
            }
        }
    }
}

/// Spawn proot with forkpty. Child gets controlling tty. Returns (child, writer).
pub fn spawn_sh_with_output(
    lines: Arc<std::sync::Mutex<VecDeque<String>>>,
    partial_line: Arc<std::sync::Mutex<String>>,
) -> Result<(ChildProcess, Option<Box<dyn Write + Send>>)> {
    let rootfs = rootfs_dir()?;
    let (argv, env) = build_exec_args(&rootfs)?;

    let argv_refs: Vec<&std::ffi::CStr> = argv.iter().map(|s| s.as_c_str()).collect();
    let env_refs: Vec<&std::ffi::CStr> = env.iter().map(|s| s.as_c_str()).collect();

    let winsize = Winsize {
        ws_row: 24,
        ws_col: 80,
        ws_xpixel: 0,
        ws_ypixel: 0,
    };
    let result = unsafe { forkpty(Some(&winsize), None).context("forkpty failed")? };

    match result {
        ForkptyResult::Child => {
            // In child: exec proot. Child already has controlling tty from forkpty.
            if execve(argv[0].as_c_str(), &argv_refs, &env_refs).is_err() {
                unsafe { nix::libc::_exit(1) };
            }
            unreachable!();
        }
        ForkptyResult::Parent { child, master } => {
            let master_read_fd = dup(&master).context("dup master for read")?.into_raw_fd();
            let master_write_fd = master.into_raw_fd();
            let master_read = unsafe { File::from_raw_fd(master_read_fd) };
            let master_write = unsafe { File::from_raw_fd(master_write_fd) };

            let lines_out = Arc::clone(&lines);
            let partial_out = Arc::clone(&partial_line);
            std::thread::spawn(move || read_pty_to_lines(master_read, lines_out, partial_out));

            Ok((ChildProcess { pid: child }, Some(Box::new(master_write))))
        }
    }
}
