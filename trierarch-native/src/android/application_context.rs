// App paths via JNI (filesDir, cacheDir, nativeLibraryDir).

use anyhow::Result;
use std::path::{Path, PathBuf};
use std::sync::Mutex;

pub const ARCH_ROOTFS_SUBDIR: &str = "arch";
pub const DEBIAN_ROOTFS_SUBDIR: &str = "debian";
pub const WINE_ROOTFS_SUBDIR: &str = "wine";
/// Sentinel file written after successful extract; if present, rootfs is ready.
pub const ROOTFS_READY_SENTINEL: &str = ".trierarch_rootfs_ok";

static APPLICATION_CONTEXT: Mutex<Option<ApplicationContext>> = Mutex::new(None);

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

pub fn arch_rootfs_dir() -> Result<PathBuf> {
    Ok(get_application_context()?.data_dir.join(ARCH_ROOTFS_SUBDIR))
}

pub fn debian_rootfs_dir() -> Result<PathBuf> {
    Ok(get_application_context()?
        .data_dir
        .join(DEBIAN_ROOTFS_SUBDIR))
}

pub fn wine_rootfs_dir() -> Result<PathBuf> {
    Ok(get_application_context()?.data_dir.join(WINE_ROOTFS_SUBDIR))
}

/// Rootfs is ready iff the extract sentinel exists.
pub fn has_rootfs(root: &Path) -> bool {
    root.join(ROOTFS_READY_SENTINEL).exists()
}
