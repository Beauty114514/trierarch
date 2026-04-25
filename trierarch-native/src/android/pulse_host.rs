//! Host PulseAudio: Unix socket `pulse-run/native` bound into the guest as [`GUEST_PULSE_UNIX_SOCKET`].
//! Guests use Unix transport only (TCP from proot is unreliable). ELF under `files/` may need the
//! system linker to execute (same pattern as `virgl_host`).

use crate::android::application_context;
use std::ffi::OsString;
use std::fs::OpenOptions;
use std::io::Write;
use std::path::PathBuf;
use std::process::{Command, Stdio};
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;
use std::time::Duration;

pub const HOST_PULSE_TCP_PORT: u16 = 4713;
pub const GUEST_PULSE_RUNTIME_MOUNT: &str = "/run/trierarch-pulse";
pub const GUEST_PULSE_UNIX_SOCKET: &str = "/run/trierarch-pulse/native";
pub const PULSE_PREFIX_SUBDIR: &str = "pulse";

fn linker() -> &'static str {
    match std::env::consts::ARCH {
        "aarch64" | "x86_64" => "/system/bin/linker64",
        "arm" | "x86" => "/system/bin/linker",
        _ => "/system/bin/linker64",
    }
}

fn exec_from_app_data(exe: &std::path::Path) -> bool {
    exe.to_string_lossy().contains("/files/")
}

pub fn host_pulse_runtime_dir(data_dir: &std::path::Path) -> PathBuf {
    data_dir.join("pulse-run")
}

pub fn guest_pulse_server_env() -> String {
    format!("unix:{}", GUEST_PULSE_UNIX_SOCKET)
}

static PULSE_SUPERVISOR_STARTED: AtomicBool = AtomicBool::new(false);

pub fn spawn_host_pulseaudio_if_present() {
    if PULSE_SUPERVISOR_STARTED
        .compare_exchange(false, true, Ordering::SeqCst, Ordering::SeqCst)
        .is_err()
    {
        return;
    }
    if let Err(e) = thread::Builder::new()
        .name("pulse-supervisor".into())
        .spawn(pulse_supervisor_main)
    {
        log::warn!("pulse: supervisor thread: {:?}", e);
        PULSE_SUPERVISOR_STARTED.store(false, Ordering::SeqCst);
    }
}

fn pulse_supervisor_main() {
    loop {
        if let Some((exe, rt, prefix, port)) = prepare() {
            run_until_exit(exe, rt, prefix, port);
        }
        thread::sleep(Duration::from_secs(3));
    }
}

fn prepare() -> Option<(PathBuf, PathBuf, Option<PathBuf>, u16)> {
    let ctx = application_context::get_application_context().ok()?;
    let rt = host_pulse_runtime_dir(&ctx.data_dir);
    let packaged = ctx.data_dir.join(PULSE_PREFIX_SUBDIR);
    let candidates = [
        packaged.join("bin/pulseaudio"),
        ctx.native_library_dir.join("pulseaudio"),
        ctx.data_dir.join("bin/pulseaudio"),
    ];
    let exe = candidates.iter().find(|p| p.is_file())?.clone();
    let prefix = exe.starts_with(&packaged).then_some(packaged);
    Some((exe, rt, prefix, HOST_PULSE_TCP_PORT))
}

fn ld_modules(prefix: Option<&PathBuf>) -> Option<OsString> {
    let root = prefix?;
    let parts = [
        root.join("lib"),
        root.join("lib/pulseaudio"),
        root.join("lib/pulseaudio/modules"),
    ];
    std::env::join_paths(parts.iter()).ok()
}

fn run_until_exit(exe: PathBuf, runtime_dir: PathBuf, pulse_prefix: Option<PathBuf>, port: u16) {
    if std::fs::create_dir_all(&runtime_dir).is_err() {
        return;
    }
    let runtime_dir = std::fs::canonicalize(&runtime_dir).unwrap_or(runtime_dir);
    let tmpdir = runtime_dir.join("tmp");
    let _ = std::fs::create_dir_all(&tmpdir);
    let unix_sock = runtime_dir.join("native");
    let _ = std::fs::remove_file(runtime_dir.join("pulse/native"));
    let _ = std::fs::remove_file(&unix_sock);
    // Avoid stale pid-file state causing "daemon already running" mis-detection.
    let _ = std::fs::remove_file(runtime_dir.join("pulse/pid"));
    let _ = std::fs::remove_file(runtime_dir.join("pulse/pid.lock"));

    let err_path = runtime_dir.join("pulseaudio-stderr.log");
    let mut log = match OpenOptions::new().create(true).append(true).open(&err_path) {
        Ok(f) => f,
        Err(_) => return,
    };
    let _ = writeln!(
        &mut log,
        "\n--- pulse {:?} exe={:?} sock={:?} ---",
        std::time::SystemTime::now(),
        exe,
        unix_sock
    );
    let _ = log.flush();
    let stderr = match log.try_clone() {
        Ok(c) => Stdio::from(c),
        Err(_) => Stdio::null(),
    };

    let mut cmd = if exec_from_app_data(&exe) {
        let mut c = Command::new(linker());
        c.arg(&exe);
        c
    } else {
        Command::new(&exe)
    };

    cmd.arg("-n")
        // Pulse's pid-file is unreliable in this environment (stale pid, /proc races).
        .arg("--use-pid-file=no")
        .arg("--disable-shm=yes")
        .arg("--exit-idle-time=-1")
        .arg("--daemonize=no")
        .arg("--log-target=stderr")
        // Keep at debug: this log is written to `pulse-run/pulseaudio-stderr.log` and is the
        // primary way to diagnose "connection refused" from guest `pactl`/libpulse.
        .arg("--log-level=debug")
        .arg("-L")
        .arg("module-null-sink sink_name=trierarch-mix")
        .arg("-L")
        .arg(format!(
            "module-native-protocol-unix socket={}",
            unix_sock.display()
        ))
        .arg("-L")
        .arg(format!(
            "module-native-protocol-tcp listen=127.0.0.1 port={} auth-anonymous=1",
            port
        ))
        .arg("-L")
        .arg("module-aaudio-sink sink_name=trierarch-out")
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
    if let Some(ld) = ld_modules(pulse_prefix.as_ref()) {
        cmd.env("LD_LIBRARY_PATH", ld);
    }

    let mut child = match cmd.spawn() {
        Ok(c) => c,
        Err(e) => {
            let _ = writeln!(&mut log, "spawn failed: {:?}", e);
            log::warn!("pulse: spawn failed: {:?}", e);
            return;
        }
    };
    let pid = child.id();
    let _ = writeln!(&mut log, "pid={}", pid);

    for _ in 0..100 {
        if unix_sock.exists() {
            break;
        }
        thread::sleep(Duration::from_millis(50));
    }
    if !unix_sock.exists() {
        log::warn!("pulse: socket missing after wait; see {:?}", err_path);
    }

    match child.wait() {
        Ok(s) => {
            let _ = writeln!(&mut log, "exit: {}", s);
            log::warn!("pulse: exited: {}", s);
        }
        Err(e) => log::warn!("pulse: wait: {:?}", e),
    }
}
