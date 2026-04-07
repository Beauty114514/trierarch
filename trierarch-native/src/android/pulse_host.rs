//! Host **PulseAudio**: AAudio sink, Unix + TCP native protocol.
//!
//! **Guest `PULSE_SERVER`** is [`GUEST_PULSE_UNIX_SOCKET`]; on the host the same inode is
//! `files/pulse-run/native`. We pass `socket=` to `module-native-protocol-unix` so PA does not pick
//! `…/pulse/native` or another default under `pa_runtime_path()`.
//!
//! TCP to `127.0.0.1` from inside proot is unreliable; Unix via this bind is the supported path.
//!
//! **Exec from `files/`:** `pulseaudio` lives under `files/pulse/...` (app data). Many Android
//! builds deny direct `execve` there (`EACCES`); we then start the PIE with `/system/bin/linker64`
//! (or 32-bit `linker`) so the dynamic loader maps the ELF instead.

use crate::android::application_context;
use std::ffi::OsString;
use std::fs::OpenOptions;
use std::io::Write;
use std::path::PathBuf;
use std::process::{Command, Stdio};
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;
use std::time::Duration;

/// TCP port for `module-native-protocol-tcp` (debug / non-proot only).
pub const HOST_PULSE_TCP_PORT: u16 = 4713;

/// Guest mount point for [`host_pulse_runtime_dir`]; keep in sync with `proot::args` `--bind`.
pub const GUEST_PULSE_RUNTIME_MOUNT: &str = "/run/trierarch-pulse";

/// In-guest path; host file is `pulse-run/native` (see `socket=` in `run_pulse_until_exit`).
pub const GUEST_PULSE_UNIX_SOCKET: &str = "/run/trierarch-pulse/native";

pub const PULSE_PREFIX_SUBDIR: &str = "pulse";

/// Bionic dynamic linker used to run a PIE/ET_DYN ELF when direct `execve` on `dataDir/files/` fails.
fn android_linker_for_arch() -> &'static str {
    match std::env::consts::ARCH {
        "aarch64" | "x86_64" => "/system/bin/linker64",
        "arm" | "x86" => "/system/bin/linker",
        _ => "/system/bin/linker64",
    }
}

/// Paths under app `files/` are often not executable via plain `execve` (policy / mount).
fn pulse_exe_needs_linker_bootstrap(exe: &std::path::Path) -> bool {
    let s = exe.to_string_lossy();
    s.contains("/files/")
}

pub fn host_pulse_runtime_dir(data_dir: &std::path::Path) -> PathBuf {
    data_dir.join("pulse-run")
}

pub fn guest_pulse_server_env() -> String {
    format!("unix:{}", GUEST_PULSE_UNIX_SOCKET)
}

/// Only one supervisor thread; it **restarts** pulse after exit (unlike `Once`, which never retries).
static PULSE_SUPERVISOR_STARTED: AtomicBool = AtomicBool::new(false);

pub fn spawn_host_pulseaudio_if_present() {
    if PULSE_SUPERVISOR_STARTED
        .compare_exchange(false, true, Ordering::SeqCst, Ordering::SeqCst)
        .is_err()
    {
        return;
    }
    if let Err(e) = thread::Builder::new()
        .name("pulseaudio-supervisor".into())
        .spawn(pulse_supervisor_main)
    {
        log::warn!("pulse_host: supervisor thread: {:?}", e);
        PULSE_SUPERVISOR_STARTED.store(false, Ordering::SeqCst);
    }
}

fn pulse_supervisor_main() {
    loop {
        match prepare_pulse_invocation() {
            Some((exe, runtime_dir, pulse_prefix, port)) => {
                run_pulse_until_exit(exe, runtime_dir, pulse_prefix, port);
            }
            None => {
                log::debug!("pulse_host: no pulseaudio binary yet; retry in 3s");
            }
        }
        thread::sleep(Duration::from_secs(3));
    }
}

fn prepare_pulse_invocation() -> Option<(PathBuf, PathBuf, Option<PathBuf>, u16)> {
    let ctx = application_context::get_application_context().ok()?;
    let runtime_dir = host_pulse_runtime_dir(&ctx.data_dir);
    let packaged = ctx.data_dir.join(PULSE_PREFIX_SUBDIR);
    let primary = packaged.join("bin").join("pulseaudio");
    let candidates = [
        primary,
        ctx.native_library_dir.join("pulseaudio"),
        ctx.data_dir.join("bin").join("pulseaudio"),
    ];
    let exe = candidates.iter().find(|p| p.is_file())?.clone();
    let pulse_prefix = if exe.starts_with(&packaged) {
        Some(packaged)
    } else {
        None
    };
    Some((exe, runtime_dir, pulse_prefix, HOST_PULSE_TCP_PORT))
}

fn ld_library_path_entries(prefix: Option<&PathBuf>) -> Option<OsString> {
    let root = prefix?;
    // `module-*.so` dlopen()s pull in `libprotocol-native.so` et al. from the same `modules/` dir;
    // Android's linker does not implicitly search beside the module, so include it in LD_LIBRARY_PATH.
    let parts = [
        root.join("lib"),
        root.join("lib/pulseaudio"),
        root.join("lib/pulseaudio/modules"),
    ];
    std::env::join_paths(parts.iter()).ok()
}

fn run_pulse_until_exit(
    exe: PathBuf,
    runtime_dir: PathBuf,
    pulse_prefix: Option<PathBuf>,
    port: u16,
) {
    if let Err(e) = std::fs::create_dir_all(&runtime_dir) {
        log::warn!("pulse_host: create runtime dir: {:?}", e);
        return;
    }
    let runtime_dir = match std::fs::canonicalize(&runtime_dir) {
        Ok(p) => p,
        Err(e) => {
            log::warn!("pulse_host: canonicalize runtime dir: {:?}; using non-canonical", e);
            runtime_dir
        }
    };

    let tmpdir = runtime_dir.join("tmp");
    let _ = std::fs::create_dir_all(&tmpdir);

    let unix_socket_host = runtime_dir.join("native");
    let _ = std::fs::remove_file(runtime_dir.join("pulse").join("native"));
    let _ = std::fs::remove_file(&unix_socket_host);

    let stderr_path = runtime_dir.join("pulseaudio-stderr.log");
    let (stderr, mut log_handle): (Stdio, Option<std::fs::File>) = match OpenOptions::new()
        .create(true)
        .append(true)
        .open(&stderr_path)
    {
        Ok(mut f) => {
            let _ = writeln!(
                f,
                "\n--- pulse_host: attempt at {:?} ---\nexe={:?}\nlinker_bootstrap={}\nsocket={:?}\nPULSE_DLPATH={:?}\nLD_LIBRARY_PATH={:?}",
                std::time::SystemTime::now(),
                exe,
                pulse_exe_needs_linker_bootstrap(&exe),
                unix_socket_host,
                pulse_prefix.as_ref().map(|p| p.join("lib/pulseaudio/modules")),
                ld_library_path_entries(pulse_prefix.as_ref())
            );
            let _ = f.flush();
            match f.try_clone() {
                Ok(clone) => (Stdio::from(clone), Some(f)),
                Err(_) => (Stdio::from(f), None),
            }
        }
        Err(e) => {
            log::warn!("pulse_host: open stderr log {:?}: {:?}", stderr_path, e);
            (Stdio::null(), None)
        }
    };

    let mut cmd = if pulse_exe_needs_linker_bootstrap(&exe) {
        let linker = android_linker_for_arch();
        if let Some(ref mut lf) = log_handle {
            let _ = writeln!(
                lf,
                "pulse_host: using {} as loader (files/ exec workaround)",
                linker
            );
        }
        let mut c = Command::new(linker);
        c.arg(&exe);
        c
    } else {
        Command::new(&exe)
    };
    cmd.arg("-n")
        // PRoot guests share the Unix socket but cannot use memfd/SHM with the host daemon; without
        // this, libpulse negotiates SHM and then fails with "Protocol error".
        .arg("--disable-shm=yes")
        .arg("--exit-idle-time=-1")
        .arg("--daemonize=no")
        .arg("--log-target=stderr")
        .arg("--log-level=debug")
        .arg("-L")
        .arg("module-null-sink sink_name=trierarch-mix")
        .arg("-L")
        .arg(format!(
            "module-native-protocol-unix socket={}",
            unix_socket_host.display()
        ))
        .arg("-L")
        .arg(format!(
            "module-native-protocol-tcp listen=127.0.0.1 port={} auth-anonymous=1",
            port
        ))
        .arg("-L")
        .arg("module-aaudio-sink sink_name=trierarch-out")
        // Pulse picks runtime directory via XDG_RUNTIME_DIR / internal defaults; force both so the
        // unix socket is always under `pulse-run/` (matches proot bind), not an inherited env path.
        .env("PULSE_RUNTIME_PATH", &runtime_dir)
        .env("XDG_RUNTIME_DIR", &runtime_dir)
        .env("HOME", &runtime_dir)
        .env("TMPDIR", &tmpdir)
        .env_remove("PULSE_SERVER")
        .env_remove("PULSE_COOKIE")
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(stderr);

    if let Some(ref pfx) = pulse_prefix {
        cmd.env("PULSE_DLPATH", pfx.join("lib/pulseaudio/modules"));
    }
    if let Some(ld) = ld_library_path_entries(pulse_prefix.as_ref()) {
        cmd.env("LD_LIBRARY_PATH", ld);
    }

    let mut child = match cmd.spawn() {
        Ok(c) => c,
        Err(e) => {
            if let Some(ref mut lf) = log_handle {
                let _ = writeln!(lf, "pulse_host: SPAWN FAILED: {:?}", e);
            }
            log::warn!("pulse_host: spawn {:?}: {:?}", exe, e);
            return;
        }
    };
    let pid = child.id();
    if let Some(ref mut lf) = log_handle {
        let _ = writeln!(lf, "pulse_host: spawned pid={}", pid);
    }
    log::info!(
        "pulse_host: pulseaudio pid={} runtime={:?}; errors -> {:?}",
        pid,
        runtime_dir,
        stderr_path
    );

    let sock = unix_socket_host;
    for i in 0..100 {
        if sock.exists() {
            log::info!("pulse_host: unix socket appeared after {}ms", i * 50);
            if let Some(ref mut lf) = log_handle {
                let _ = writeln!(lf, "pulse_host: unix socket appeared after {}ms", i * 50);
            }
            break;
        }
        thread::sleep(Duration::from_millis(50));
    }
    if !sock.exists() {
        log::warn!(
            "pulse_host: still no {:?} after wait; check {:?}",
            sock,
            stderr_path
        );
    }

    match child.wait() {
        Ok(s) => {
            if let Some(ref mut lf) = log_handle {
                let _ = writeln!(lf, "pulse_host: pulseaudio (pid={}) exited: {}", pid, s);
            }
            log::warn!("pulse_host: pulseaudio exited: {}", s);
        }
        Err(e) => {
            if let Some(ref mut lf) = log_handle {
                let _ = writeln!(lf, "pulse_host: wait error (pid={}): {:?}", pid, e);
            }
            log::warn!("pulse_host: wait: {:?}", e);
        }
    }
}
