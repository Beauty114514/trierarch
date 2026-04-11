//! Host `virgl_test_server_android`: Unix socket for guest GL/Vulkan (virpipe + Venus).
//! With `--venus`, child processes may survive the parent; `stop_if_running` kills the supervised
//! process, then matches this UID in `/proc` and SIGKILLs stray `virgl_*` helpers.

use crate::android::application_context;
use std::fs::OpenOptions;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::Mutex;

const VIRGL: &str = "virgl";

pub fn host_virgl_runtime_dir(data_dir: &Path) -> PathBuf {
    data_dir.join("virgl-run")
}

fn exe_candidates(ctx: &application_context::ApplicationContext) -> [PathBuf; 3] {
    [
        ctx.data_dir.join(VIRGL).join("bin/virgl_test_server_android"),
        ctx.native_library_dir.join("virgl_test_server_android"),
        ctx.data_dir.join("bin/virgl_test_server_android"),
    ]
}

fn linker() -> &'static str {
    match std::env::consts::ARCH {
        "aarch64" | "x86_64" => "/system/bin/linker64",
        "arm" | "x86" => "/system/bin/linker",
        _ => "/system/bin/linker64",
    }
}

fn exec_from_app_data(exe: &Path) -> bool {
    exe.to_string_lossy().contains("/files/")
}

static CHILD: Mutex<Option<Child>> = Mutex::new(None);

fn uid_line(status: &str) -> Option<u32> {
    let line = status.lines().find(|l| l.starts_with("Uid:"))?;
    line.split_whitespace().nth(1)?.parse().ok()
}

fn pgid_line(status: &str) -> Option<i32> {
    let line = status.lines().find(|l| l.starts_with("Pgid:"))?;
    line.split_whitespace().nth(1)?.parse().ok()
}

fn kill_stragglers() {
    let me = unsafe { libc::getuid() };
    let Ok(rd) = std::fs::read_dir("/proc") else {
        return;
    };
    for e in rd.flatten() {
        let Ok(pid) = e.file_name().to_string_lossy().parse::<libc::pid_t>() else {
            continue;
        };
        if pid <= 1 {
            continue;
        }
        let p = e.path();
        let Ok(st) = std::fs::read_to_string(p.join("status")) else {
            continue;
        };
        if uid_line(&st) != Some(me) {
            continue;
        }
        let Ok(raw) = std::fs::read(p.join("cmdline")) else {
            continue;
        };
        let cmd = String::from_utf8_lossy(&raw);
        if !cmd.contains("virgl_test_server_android") && !cmd.contains("virgl_render_server") {
            continue;
        }
        unsafe {
            libc::kill(pid, libc::SIGKILL);
        }
    }
}

pub fn stop_if_running() {
    let mut g = match CHILD.lock() {
        Ok(x) => x,
        Err(_) => {
            kill_stragglers();
            rm_socket();
            return;
        }
    };
    if let Some(mut ch) = g.take() {
        let pid = ch.id() as i32;
        if pid > 0 {
            let kill_pg = std::fs::read_to_string(format!("/proc/{pid}/status"))
                .ok()
                .and_then(|s| pgid_line(&s))
                .map(|pg| pg == pid)
                .unwrap_or(false);
            if kill_pg {
                unsafe {
                    libc::kill(-pid, libc::SIGKILL);
                }
            } else {
                let _ = ch.kill();
            }
        } else {
            let _ = ch.kill();
        }
        let _ = ch.wait();
    }
    kill_stragglers();
    rm_socket();
}

fn rm_socket() {
    if let Ok(ctx) = application_context::get_application_context() {
        let _ = std::fs::remove_file(host_virgl_runtime_dir(&ctx.data_dir).join("vtest.sock"));
    }
}

pub fn start_if_possible() {
    let ctx = match application_context::get_application_context() {
        Ok(c) => c,
        Err(_) => return,
    };
    {
        let g = match CHILD.lock() {
            Ok(x) => x,
            Err(_) => return,
        };
        if g.is_some() {
            return;
        }
    }

    let Some(exe) = exe_candidates(&ctx).into_iter().find(|p| p.is_file()) else {
        log::warn!("virgl: virgl_test_server_android not found");
        return;
    };

    let rt = host_virgl_runtime_dir(&ctx.data_dir);
    if std::fs::create_dir_all(&rt).is_err() {
        return;
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        let _ = std::fs::set_permissions(&rt, std::fs::Permissions::from_mode(0o700));
    }
    let rt = std::fs::canonicalize(&rt).unwrap_or(rt);
    let sock = rt.join("vtest.sock");
    let _ = std::fs::remove_file(&sock);

    let log_path = rt.join("virgl_host_stderr.log");
    let log_file = OpenOptions::new()
        .create(true)
        .append(true)
        .open(&log_path)
        .ok();
    let stderr = log_file
        .as_ref()
        .and_then(|f| f.try_clone().ok())
        .map(Stdio::from)
        .unwrap_or(Stdio::null());
    let stdout = log_file
        .as_ref()
        .and_then(|f| f.try_clone().ok())
        .map(Stdio::from)
        .unwrap_or(Stdio::null());

    let mut cmd = if exec_from_app_data(&exe) {
        let mut c = Command::new(linker());
        c.arg(&exe);
        c
    } else {
        Command::new(&exe)
    };

    #[cfg(unix)]
    {
        use std::os::unix::process::CommandExt;
        unsafe {
            cmd.pre_exec(|| {
                let _ = libc::setpgid(0, 0);
                Ok(())
            });
        }
    }

    cmd.arg("--use-egl-surfaceless")
        .arg("--use-gles")
        .arg("--venus")
        .arg("--socket-path")
        .arg(sock.as_os_str());
    cmd.current_dir(&rt);
    let rts = rt.to_string_lossy().to_string();
    cmd.env("XDG_RUNTIME_DIR", &rts);
    cmd.env("TMPDIR", &rts);

    let render = ctx.data_dir.join(VIRGL).join("bin/virgl_render_server");
    if render.is_file() {
        cmd.env(
            "RENDER_SERVER_EXEC_PATH",
            render.to_string_lossy().as_ref(),
        );
    }

    let angle_dir = ctx.data_dir.join(VIRGL).join("angle/vulkan");
    let angle_resolved = angle_dir.is_dir().then(|| std::fs::canonicalize(&angle_dir).unwrap_or(angle_dir));
    if let Some(ref p) = angle_resolved {
        cmd.env("ANGLE_LIBS_DIR", p.to_string_lossy().as_ref());
    }

    let lib = ctx.data_dir.join(VIRGL).join("lib");
    let bin = exe.parent().unwrap_or(Path::new("."));
    let mut ld: Vec<String> = vec![
        lib.to_string_lossy().into_owned(),
        bin.to_string_lossy().into_owned(),
    ];
    if let Some(ref p) = angle_resolved {
        ld.push(p.to_string_lossy().into_owned());
    }
    ld.push(ctx.native_library_dir.to_string_lossy().into_owned());
    ld.push(
        if cfg!(target_pointer_width = "64") {
            "/system/lib64"
        } else {
            "/system/lib"
        }
        .into(),
    );
    if let Ok(x) = std::env::var("LD_LIBRARY_PATH") {
        if !x.is_empty() {
            ld.push(x);
        }
    }
    cmd.env("LD_LIBRARY_PATH", ld.join(":"));

    cmd.stdin(Stdio::null()).stdout(stdout).stderr(stderr);

    match cmd.spawn() {
        Ok(mut child) => {
            std::thread::sleep(std::time::Duration::from_millis(200));
            match child.try_wait() {
                Ok(Some(st)) => {
                    log::warn!("virgl: process exited immediately ({:?})", st);
                    if let Ok(mut f) = OpenOptions::new().create(true).append(true).open(&log_path) {
                        let _ = writeln!(f, "early exit: {:?}", st);
                    }
                }
                Ok(None) => {
                    if let Ok(mut g) = CHILD.lock() {
                        *g = Some(child);
                    }
                }
                Err(e) => log::warn!("virgl: try_wait: {:?}", e),
            }
        }
        Err(e) => {
            log::warn!("virgl: spawn failed: {:?}", e);
            if let Ok(mut f) = OpenOptions::new().create(true).append(true).open(&log_path) {
                let _ = writeln!(f, "spawn: {:?}", e);
            }
        }
    }
}
