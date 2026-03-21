//! Shared state for JNI bridge. Holds lines, partial_line, stdin for proot PTY.

use super::android::application_context;
use super::android::proot;
use anyhow::Result;
use std::collections::VecDeque;
use std::io::Write;
use std::path::PathBuf;
use std::sync::{Arc, Mutex};

static LINES: Mutex<Option<Arc<Mutex<VecDeque<String>>>>> = Mutex::new(None);
static PARTIAL_LINE: Mutex<Option<Arc<Mutex<String>>>> = Mutex::new(None);
static STDIN: Mutex<Option<Arc<Mutex<Option<Box<dyn Write + Send>>>>>> = Mutex::new(None);
static CHILD: Mutex<Option<proot::ChildProcess>> = Mutex::new(None);

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
    // Only create PTY state on first init. Re-init (e.g. config change) updates paths but keeps
    // existing terminal buffers so a running proot read thread is not orphaned and UI does not go blank.
    let mut lines_guard = LINES
        .lock()
        .map_err(|e| anyhow::anyhow!("LINES lock: {:?}", e))?;
    if lines_guard.is_none() {
        let lines = Arc::new(Mutex::new(VecDeque::new()));
        let partial_line = Arc::new(Mutex::new(String::new()));
        let stdin = Arc::new(Mutex::new(None));
        *lines_guard = Some(lines);
        drop(lines_guard);
        *PARTIAL_LINE
            .lock()
            .map_err(|e| anyhow::anyhow!("PARTIAL_LINE lock: {:?}", e))? = Some(partial_line);
        *STDIN
            .lock()
            .map_err(|e| anyhow::anyhow!("STDIN lock: {:?}", e))? = Some(stdin);
    }
    Ok(())
}

pub fn has_rootfs() -> bool {
    application_context::rootfs_dir()
        .map(|root| application_context::has_rootfs(&root))
        .unwrap_or(false)
}

pub fn spawn_proot() -> Result<()> {
    // Drop any existing proot process and stdin so we don't have two processes or writers (e.g. retry after failure or re-init).
    {
        let _old = CHILD.lock().ok().and_then(|mut g| g.take());
        let stdin_holder = STDIN.lock().ok().and_then(|g| g.clone());
        if let Some(ref h) = stdin_holder {
            let _ = h.lock().ok().map(|mut g| *g = None);
        }
    }
    let lines = LINES
        .lock()
        .map_err(|e| anyhow::anyhow!("LINES lock: {:?}", e))?
        .clone()
        .ok_or_else(|| anyhow::anyhow!("jni_context not initialized"))?;
    let partial_line = PARTIAL_LINE
        .lock()
        .map_err(|e| anyhow::anyhow!("PARTIAL_LINE lock: {:?}", e))?
        .clone()
        .ok_or_else(|| anyhow::anyhow!("jni_context not initialized"))?;
    let stdin_holder = STDIN
        .lock()
        .map_err(|e| anyhow::anyhow!("STDIN lock: {:?}", e))?
        .clone()
        .ok_or_else(|| anyhow::anyhow!("jni_context not initialized"))?;

    let (child, writer) = proot::spawn_sh_with_output(lines, partial_line)?;
    *stdin_holder
        .lock()
        .map_err(|e| anyhow::anyhow!("stdin lock: {:?}", e))? = writer;
    *CHILD
        .lock()
        .map_err(|e| anyhow::anyhow!("CHILD lock: {:?}", e))? = Some(child);
    Ok(())
}

pub fn lines() -> Arc<Mutex<VecDeque<String>>> {
    LINES
        .lock()
        .ok()
        .and_then(|g| g.clone())
        .unwrap_or_else(|| Arc::new(Mutex::new(VecDeque::new())))
}

pub fn partial_line() -> Arc<Mutex<String>> {
    PARTIAL_LINE
        .lock()
        .ok()
        .and_then(|g| g.clone())
        .unwrap_or_else(|| Arc::new(Mutex::new(String::new())))
}

pub fn stdin() -> Arc<Mutex<Option<Box<dyn Write + Send>>>> {
    STDIN
        .lock()
        .ok()
        .and_then(|g| g.clone())
        .unwrap_or_else(|| Arc::new(Mutex::new(None)))
}
