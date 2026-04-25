//! PTY sessions: stdin, master fd, reader thread to Java.

use super::android::application_context;
use super::android::proot;
use anyhow::{Context, Result};
use jni::objects::GlobalRef;
use jni::objects::JClass;
use jni::objects::JValue;
use jni::JNIEnv;
use jni::JavaVM;
use std::collections::BTreeMap;
use std::io::{Read, Write};
use std::os::unix::io::RawFd;
use std::path::PathBuf;
use std::sync::{Mutex, OnceLock};

struct PtySession {
    _child: proot::ChildProcess,
    stdin: Box<dyn Write + Send>,
    master_fd: RawFd,
}

static SESSIONS: Mutex<BTreeMap<i32, PtySession>> = Mutex::new(BTreeMap::new());

static JAVA_VM: OnceLock<JavaVM> = OnceLock::new();
static PTY_RELAY_CLASS: OnceLock<GlobalRef> = OnceLock::new();

pub fn init(
    data_dir: PathBuf,
    cache_dir: PathBuf,
    native_library_dir: PathBuf,
    external_storage_path: Option<PathBuf>,
) -> Result<()> {
    application_context::ApplicationContext::init_from_paths(
        data_dir,
        cache_dir,
        native_library_dir,
        external_storage_path,
    )?;
    Ok(())
}

pub fn init_pty_output_jni(env: &mut JNIEnv) -> Result<()> {
    if JAVA_VM.get().is_none() {
        let vm = env.get_java_vm().context("JavaVM")?;
        let _ = JAVA_VM.set(vm);
    }
    if PTY_RELAY_CLASS.get().is_none() {
        let cls = env
            .find_class("app/trierarch/PtyOutputRelay")
            .context("find_class PtyOutputRelay")?;
        let g = env
            .new_global_ref(&cls)
            .context("global_ref PtyOutputRelay")?;
        let _ = PTY_RELAY_CLASS.set(g);
    }
    Ok(())
}

pub fn has_arch_rootfs() -> bool {
    application_context::arch_rootfs_dir()
        .map(|root| application_context::has_rootfs(&root))
        .unwrap_or(false)
}

pub fn has_debian_rootfs() -> bool {
    application_context::debian_rootfs_dir()
        .map(|root| application_context::has_rootfs(&root))
        .unwrap_or(false)
}

pub fn has_wine_rootfs() -> bool {
    application_context::wine_rootfs_dir()
        .map(|root| application_context::has_rootfs(&root))
        .unwrap_or(false)
}

#[repr(i32)]
pub enum RootfsKind {
    Arch = 0,
    Debian = 1,
    Wine = 2,
}

fn rootfs_dir_for_kind(kind: i32) -> Result<std::path::PathBuf> {
    match kind {
        x if x == RootfsKind::Arch as i32 => application_context::arch_rootfs_dir(),
        x if x == RootfsKind::Debian as i32 => application_context::debian_rootfs_dir(),
        x if x == RootfsKind::Wine as i32 => application_context::wine_rootfs_dir(),
        _ => application_context::arch_rootfs_dir(),
    }
}

pub fn spawn_session(session_id: i32, initial_rows: u16, initial_cols: u16) -> Result<()> {
    let mut map = SESSIONS
        .lock()
        .map_err(|e| anyhow::anyhow!("SESSIONS lock: {:?}", e))?;
    if map.contains_key(&session_id) {
        return Ok(());
    }
    let (child, read_file, stdin, master_fd) = proot::fork_pty_shell(initial_rows, initial_cols)?;
    map.insert(
        session_id,
        PtySession {
            _child: child,
            stdin,
            master_fd,
        },
    );
    drop(map);
    std::thread::spawn(move || {
        pty_master_reader_loop(session_id, read_file);
    });
    Ok(())
}

pub fn spawn_session_in_rootfs(
    session_id: i32,
    initial_rows: u16,
    initial_cols: u16,
    rootfs_kind: i32,
) -> Result<()> {
    let mut map = SESSIONS
        .lock()
        .map_err(|e| anyhow::anyhow!("SESSIONS lock: {:?}", e))?;
    if map.contains_key(&session_id) {
        return Ok(());
    }
    let rootfs = rootfs_dir_for_kind(rootfs_kind)?;
    let (child, read_file, stdin, master_fd) =
        proot::fork_pty_shell_in_rootfs(&rootfs, initial_rows, initial_cols)?;
    map.insert(
        session_id,
        PtySession {
            _child: child,
            stdin,
            master_fd,
        },
    );
    drop(map);
    std::thread::spawn(move || {
        pty_master_reader_loop(session_id, read_file);
    });
    Ok(())
}

pub fn close_session(session_id: i32) -> Result<()> {
    let mut map = SESSIONS
        .lock()
        .map_err(|e| anyhow::anyhow!("SESSIONS lock: {:?}", e))?;
    map.remove(&session_id);
    Ok(())
}

/// Close all sessions (renderer mode change: env is fixed at spawn).
pub fn close_all_sessions() -> Result<()> {
    let mut map = SESSIONS
        .lock()
        .map_err(|e| anyhow::anyhow!("SESSIONS lock: {:?}", e))?;
    map.clear();
    Ok(())
}

pub fn is_session_alive(session_id: i32) -> bool {
    SESSIONS
        .lock()
        .ok()
        .map(|m| m.contains_key(&session_id))
        .unwrap_or(false)
}

pub fn write_input(session_id: i32, bytes: &[u8]) -> Result<()> {
    let mut map = SESSIONS
        .lock()
        .map_err(|e| anyhow::anyhow!("SESSIONS lock: {:?}", e))?;
    let s = map
        .get_mut(&session_id)
        .ok_or_else(|| anyhow::anyhow!("no session {}", session_id))?;
    s.stdin
        .write_all(bytes)
        .map_err(|e| anyhow::anyhow!("stdin write: {}", e))?;
    s.stdin
        .flush()
        .map_err(|e| anyhow::anyhow!("stdin flush: {}", e))?;
    Ok(())
}

pub fn set_pty_window_size(session_id: i32, rows: u16, cols: u16) -> Result<()> {
    let map = SESSIONS
        .lock()
        .map_err(|e| anyhow::anyhow!("SESSIONS lock: {:?}", e))?;
    let fd = map
        .get(&session_id)
        .map(|s| s.master_fd)
        .ok_or_else(|| anyhow::anyhow!("no session {}", session_id))?;
    let ws = libc::winsize {
        ws_row: rows,
        ws_col: cols,
        ws_xpixel: 0,
        ws_ypixel: 0,
    };
    unsafe {
        if libc::ioctl(fd, libc::TIOCSWINSZ, &ws as *const _) < 0 {
            let err = std::io::Error::last_os_error();
            return Err(anyhow::anyhow!("TIOCSWINSZ: {}", err));
        }
    }
    Ok(())
}

pub fn pty_master_reader_loop(session_id: i32, mut master_read: std::fs::File) {
    let mut buf = [0u8; 4096];
    loop {
        match master_read.read(&mut buf) {
            Ok(0) => break,
            Ok(n) => {
                post_pty_chunk_to_java(session_id, &buf[..n]);
            }
            Err(_) => break,
        }
    }
}

fn post_pty_chunk_to_java(session_id: i32, bytes: &[u8]) {
    let Some(vm) = JAVA_VM.get() else {
        return;
    };
    let Some(class_ref) = PTY_RELAY_CLASS.get() else {
        return;
    };
    let Ok(mut env) = vm.attach_current_thread_as_daemon() else {
        return;
    };
    let Ok(arr) = env.byte_array_from_slice(bytes) else {
        return;
    };
    let Ok(local_cls) = env.new_local_ref(class_ref.as_obj()) else {
        return;
    };
    let cls = JClass::from(local_cls);
    if let Err(e) = env.call_static_method(
        cls,
        "onPtyOutputChunk",
        "(I[B)V",
        &[JValue::Int(session_id), JValue::Object(arr.as_ref())],
    ) {
        log::warn!("onPtyOutputChunk: {:?}", e);
    }
}
