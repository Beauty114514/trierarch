// App paths via JNI (filesDir, cacheDir, nativeLibraryDir).

use anyhow::Result;
use std::path::{Path, PathBuf};
use std::sync::Mutex;

pub const ARCH_ROOTFS_SUBDIR: &str = "arch";
/// Sentinel file written after successful extract; if present, rootfs is ready.
pub const ROOTFS_READY_SENTINEL: &str = ".trierarch_rootfs_ok";

static APPLICATION_CONTEXT: Mutex<Option<ApplicationContext>> = Mutex::new(None);
static RENDERER_MODE: Mutex<RendererMode> = Mutex::new(RendererMode::LlvmPipe);

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum RendererMode {
    LlvmPipe,
    Universal,
}

impl RendererMode {
    pub fn from_str(s: &str) -> RendererMode {
        match s.trim().to_ascii_uppercase().as_str() {
            "UNIVERSAL" | "VIRGL" | "VENUS" => RendererMode::Universal,
            _ => RendererMode::LlvmPipe,
        }
    }

    pub fn as_env_value(&self) -> &'static str {
        match self {
            RendererMode::LlvmPipe => "LLVMPIPE",
            RendererMode::Universal => "UNIVERSAL",
        }
    }

    pub fn needs_virgl_server(&self) -> bool {
        matches!(self, RendererMode::Universal)
    }
}

#[derive(Clone, Debug)]
pub struct ApplicationContext {
    pub cache_dir: PathBuf,
    pub data_dir: PathBuf,
    pub native_library_dir: PathBuf,
    /// If set, proot binds this into the guest as `/android` and `/root/Android`.
    pub external_storage_path: Option<PathBuf>,
}

impl ApplicationContext {
    /// Initialize from paths (JNI). Call before any other native method.
    pub fn init_from_paths(
        data_dir: PathBuf,
        cache_dir: PathBuf,
        native_library_dir: PathBuf,
        external_storage_path: Option<PathBuf>,
    ) -> Result<()> {
        let ctx = ApplicationContext {
            cache_dir,
            data_dir,
            native_library_dir,
            external_storage_path,
        };
        *APPLICATION_CONTEXT
            .lock()
            .map_err(|e| anyhow::anyhow!("ApplicationContext lock poisoned: {:?}", e))? = Some(ctx);
        Ok(())
    }
}

pub fn get_application_context() -> Result<ApplicationContext> {
    APPLICATION_CONTEXT
        .lock()
        .map_err(|e| anyhow::anyhow!("ApplicationContext lock poisoned: {:?}", e))?
        .clone()
        .ok_or_else(|| anyhow::anyhow!("ApplicationContext not initialized"))
}

pub fn set_renderer_mode(mode: RendererMode) -> Result<()> {
    *RENDERER_MODE
        .lock()
        .map_err(|e| anyhow::anyhow!("RendererMode lock poisoned: {:?}", e))? = mode;
    Ok(())
}

pub fn get_renderer_mode() -> Result<RendererMode> {
    Ok(*RENDERER_MODE
        .lock()
        .map_err(|e| anyhow::anyhow!("RendererMode lock poisoned: {:?}", e))?)
}

pub fn rootfs_dir() -> Result<PathBuf> {
    Ok(get_application_context()?.data_dir.join(ARCH_ROOTFS_SUBDIR))
}

/// Rootfs is ready iff the extract sentinel exists.
pub fn has_rootfs(root: &Path) -> bool {
    root.join(ROOTFS_READY_SENTINEL).exists()
}
