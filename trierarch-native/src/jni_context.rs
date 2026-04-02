//! Shared state for JNI bridge: PTY stdin, child process, Java PTY relay, winsize.
//!
//! - `init()` sets application paths and stdin Arc once per process.
//! - `spawn_proot(rows, cols)` creates the child + PTY once (idempotent); a background thread streams PTY output to Java.
//! - `init_pty_output_jni()` must run on the JNI main thread after `init()` so PTY chunks can be delivered.

use super::android::application_context;
use super::android::proot;
use anyhow::{Context, Result};
use jni::objects::GlobalRef;
use jni::objects::JClass;
use jni::objects::JValue;
use jni::JNIEnv;
use jni::JavaVM;
use std::io::{Read, Write};
use std::os::unix::io::RawFd;
use std::path::PathBuf;
use std::sync::{Arc, Mutex, OnceLock};

static STDIN: Mutex<Option<Arc<Mutex<Option<Box<dyn Write + Send>>>>>> = Mutex::new(None);
static CHILD: Mutex<Option<proot::ChildProcess>> = Mutex::new(None);
static PTY_MASTER_FD: Mutex<Option<RawFd>> = Mutex::new(None);

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
    let mut stdin_guard = STDIN
        .lock()
        .map_err(|e| anyhow::anyhow!("STDIN lock: {:?}", e))?;
    if stdin_guard.is_none() {
        let stdin = Arc::new(Mutex::new(None));
        *stdin_guard = Some(stdin);
    }
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
        let g = env.new_global_ref(&cls).context("global_ref PtyOutputRelay")?;
        let _ = PTY_RELAY_CLASS.set(g);
    }
    Ok(())
}

pub fn has_rootfs() -> bool {
    application_context::rootfs_dir()
        .map(|root| application_context::has_rootfs(&root))
        .unwrap_or(false)
}

pub fn spawn_proot(initial_rows: u16, initial_cols: u16) -> Result<()> {
    if CHILD
        .lock()
        .map_err(|e| anyhow::anyhow!("CHILD lock: {:?}", e))?
        .is_some()
    {
        return Ok(());
    }
    let stdin_holder = STDIN
        .lock()
        .map_err(|e| anyhow::anyhow!("STDIN lock: {:?}", e))?
        .clone()
        .ok_or_else(|| anyhow::anyhow!("jni_context not initialized"))?;

    let (child, writer) = proot::spawn_sh_with_pty_reader(initial_rows, initial_cols)?;
    *stdin_holder
        .lock()
        .map_err(|e| anyhow::anyhow!("stdin lock: {:?}", e))? = writer;
    *CHILD
        .lock()
        .map_err(|e| anyhow::anyhow!("CHILD lock: {:?}", e))? = Some(child);
    Ok(())
}

pub fn is_proot_spawned() -> bool {
    CHILD
        .lock()
        .ok()
        .map(|g| g.is_some())
        .unwrap_or(false)
}

pub fn stdin() -> Arc<Mutex<Option<Box<dyn Write + Send>>>> {
    STDIN
        .lock()
        .ok()
        .and_then(|g| g.clone())
        .unwrap_or_else(|| Arc::new(Mutex::new(None)))
}

pub fn register_pty_master_fd(fd: RawFd) -> Result<()> {
    let mut g = PTY_MASTER_FD
        .lock()
        .map_err(|e| anyhow::anyhow!("PTY_MASTER_FD: {:?}", e))?;
    *g = Some(fd);
    Ok(())
}

pub fn set_pty_window_size(rows: u16, cols: u16) -> Result<()> {
    let fd = PTY_MASTER_FD
        .lock()
        .map_err(|e| anyhow::anyhow!("PTY_MASTER_FD: {:?}", e))?
        .ok_or_else(|| anyhow::anyhow!("no pty fd"))?;
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

pub fn pty_master_reader_loop(mut master_read: std::fs::File) {
    let mut buf = [0u8; 4096];
    loop {
        match master_read.read(&mut buf) {
            Ok(0) => break,
            Ok(n) => {
                post_pty_chunk_to_java(&buf[..n]);
            }
            Err(_) => break,
        }
    }
}

fn post_pty_chunk_to_java(bytes: &[u8]) {
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
        "([B)V",
        &[JValue::Object(arr.as_ref())],
    ) {
        log::warn!("onPtyOutputChunk: {:?}", e);
    }
}
