//! Download verified tarball, extract to staging, write [`ROOTFS_READY_SENTINEL`], rename into place.

use super::application_context::{
    arch_rootfs_dir, debian_rootfs_dir, get_application_context, has_rootfs, wine_rootfs_dir,
    ROOTFS_READY_SENTINEL,
};
use anyhow::{Context, Result};
use std::sync::Mutex;
use std::time::{SystemTime, UNIX_EPOCH};

mod download;
mod extract;

static DOWNLOAD_LOCK: Mutex<()> = Mutex::new(());
// proot-distro plugin (master as of 2026-04): Arch Linux rootfs is served via easycli mirror.
const ARCH_TARBALL_NAME: &str = "archlinux-aarch64-pd-v4.37.0.tar.xz";
const ARCH_TARBALL_SHA256: &str = "718151cc4adad701223c689a7e4690cb7710b7b16e9b23617b671856ff04d563";
const ARCH_TARBALL_URL: &str = "https://easycli.sh/proot-distro/archlinux-aarch64-pd-v4.37.0.tar.xz";

// Debian plugin (proot-distro master as of 2026-04): Debian (trixie) stable.
const DEBIAN_TARBALL_NAME: &str = "debian-trixie-aarch64-pd-v4.37.0.tar.xz";
const DEBIAN_TARBALL_SHA256: &str = "9bd3b19ff7cd300c7c7bf33124b726eb199f4bab9a3b1472f34749c6d12c9195";
const DEBIAN_TARBALL_URL: &str = "https://easycli.sh/proot-distro/debian-trixie-aarch64-pd-v4.37.0.tar.xz";

// Wine plugin: currently the same base as Debian (trixie) but extracted into its own rootfs dir.
const WINE_TARBALL_NAME: &str = DEBIAN_TARBALL_NAME;
const WINE_TARBALL_SHA256: &str = DEBIAN_TARBALL_SHA256;
const WINE_TARBALL_URL: &str = DEBIAN_TARBALL_URL;

pub type ProgressFn = Box<dyn Fn(u32, &str) + Send>;

pub fn ensure_arch_rootfs_with_progress(progress: Option<ProgressFn>) -> Result<()> {
    let _guard = DOWNLOAD_LOCK
        .lock()
        .map_err(|e| anyhow::anyhow!("download lock poisoned: {:?}", e))?;
    let ctx = get_application_context()?;
    let rootfs_path = arch_rootfs_dir()?;
    let cache_dir = &ctx.cache_dir;

    if has_rootfs(&rootfs_path) {
        return Ok(());
    }

    let report = |pct: u32, msg: &str| {
        if let Some(ref f) = progress {
            f(pct, msg);
        }
    };

    std::fs::create_dir_all(cache_dir).context("create cache dir")?;
    let tarball_path = cache_dir.join(ARCH_TARBALL_NAME);
    let tarball_tmp = cache_dir.join(format!("{}.tmp", ARCH_TARBALL_NAME));
    let temp_extract = rootfs_path
        .parent()
        .map(|p| p.join("arch_extract_tmp"))
        .unwrap_or_else(|| rootfs_path.join("arch_extract_tmp"));
    let staging_rootfs_path = rootfs_path
        .parent()
        .map(|p| p.join("arch.new"))
        .unwrap_or_else(|| rootfs_path.with_extension("new"));
    let rootfs_name = rootfs_path
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("arch")
        .to_string();

    loop {
        if tarball_path.exists() {
            let existing =
                download::sha256_file(&tarball_path).context("sha256 existing tarball")?;
            if existing == ARCH_TARBALL_SHA256 {
                break;
            }
            log::warn!("rootfs tarball sha256 mismatch, redownloading");
            let _ = std::fs::remove_file(&tarball_path);
        }

        report(0, "Downloading Arch...");
        let _ = std::fs::remove_file(&tarball_tmp);
        match download::download_tarball_with_progress(
            &tarball_tmp,
            &tarball_path,
            0,
            70,
            &report,
            ARCH_TARBALL_URL,
            ARCH_TARBALL_SHA256,
            "Downloading Arch",
        ) {
            Ok(()) => break,
            Err(e) => {
                log::warn!("rootfs download failed, retry: {:?}", e);
                let _ = std::fs::remove_file(&tarball_tmp);
                let _ = std::fs::remove_file(&tarball_path);
            }
        }
    }

    loop {
        report(70, "Extracting Arch...");
        if let Some(parent) = rootfs_path.parent() {
            let _ = std::fs::create_dir_all(parent);
        }
        let _ = std::fs::remove_dir_all(&temp_extract);
        let _ = std::fs::remove_dir_all(&staging_rootfs_path);

        match extract::extract_tarball(&tarball_path, &staging_rootfs_path, &temp_extract) {
            Ok(()) => {
                extract::validate_rootfs_structure(&staging_rootfs_path)
                    .context("validate extracted rootfs structure")?;
                break;
            }
            Err(e) => {
                log::warn!("rootfs extract failed, retry: {:?}", e);
                let _ = std::fs::remove_dir_all(&temp_extract);
                let _ = std::fs::remove_dir_all(&staging_rootfs_path);
            }
        }
    }

    let sentinel_path = staging_rootfs_path.join(ROOTFS_READY_SENTINEL);
    std::fs::write(&sentinel_path, b"").context("write rootfs ready sentinel")?;
    if !sentinel_path.exists() {
        anyhow::bail!("sentinel file not present after write");
    }

    let had_old = rootfs_path.exists();
    let mut backup_rootfs_path: Option<std::path::PathBuf> = None;
    if had_old {
        let ts = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis();
        let backup_name = format!("{}.bak.{}", rootfs_name, ts);
        let parent = rootfs_path
            .parent()
            .ok_or_else(|| anyhow::anyhow!("rootfs_path has no parent"))?;
        let backup_path = parent.join(backup_name);
        std::fs::rename(&rootfs_path, &backup_path).context("rename old rootfs -> backup")?;
        backup_rootfs_path = Some(backup_path);
    }

    match std::fs::rename(&staging_rootfs_path, &rootfs_path) {
        Ok(()) => {
            if let Some(ref backup_path) = backup_rootfs_path {
                let _ = std::fs::remove_dir_all(backup_path);
            }
        }
        Err(e) => {
            if let Some(ref backup_path) = backup_rootfs_path {
                let _ = std::fs::rename(backup_path, &rootfs_path)
                    .context("rollback backup rootfs -> arch")?;
            }
            return Err(e).context("atomic rootfs swap failed");
        }
    }

    let _ = std::fs::remove_file(&tarball_path);
    report(100, "Done");
    Ok(())
}

pub fn ensure_debian_rootfs_with_progress(progress: Option<ProgressFn>) -> Result<()> {
    let _guard = DOWNLOAD_LOCK
        .lock()
        .map_err(|e| anyhow::anyhow!("download lock poisoned: {:?}", e))?;
    let ctx = get_application_context()?;
    let rootfs_path = debian_rootfs_dir()?;
    let cache_dir = &ctx.cache_dir;

    if has_rootfs(&rootfs_path) {
        return Ok(());
    }

    let report = |pct: u32, msg: &str| {
        if let Some(ref f) = progress {
            f(pct, msg);
        }
    };

    std::fs::create_dir_all(cache_dir).context("create cache dir")?;
    let tarball_path = cache_dir.join(DEBIAN_TARBALL_NAME);
    let tarball_tmp = cache_dir.join(format!("{}.tmp", DEBIAN_TARBALL_NAME));
    let temp_extract = rootfs_path
        .parent()
        .map(|p| p.join("debian_extract_tmp"))
        .unwrap_or_else(|| rootfs_path.join("debian_extract_tmp"));
    let staging_rootfs_path = rootfs_path
        .parent()
        .map(|p| p.join("debian.new"))
        .unwrap_or_else(|| rootfs_path.with_extension("new"));
    let rootfs_name = rootfs_path
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("debian")
        .to_string();

    loop {
        if tarball_path.exists() {
            let existing =
                download::sha256_file(&tarball_path).context("sha256 existing tarball")?;
            if existing == DEBIAN_TARBALL_SHA256 {
                break;
            }
            log::warn!("debian rootfs tarball sha256 mismatch, redownloading");
            let _ = std::fs::remove_file(&tarball_path);
        }

        report(0, "Downloading Debian...");
        let _ = std::fs::remove_file(&tarball_tmp);
        match download::download_tarball_with_progress(
            &tarball_tmp,
            &tarball_path,
            0,
            70,
            &report,
            DEBIAN_TARBALL_URL,
            DEBIAN_TARBALL_SHA256,
            "Downloading Debian",
        ) {
            Ok(()) => break,
            Err(e) => {
                log::warn!("debian rootfs download failed, retry: {:?}", e);
                let _ = std::fs::remove_file(&tarball_tmp);
                let _ = std::fs::remove_file(&tarball_path);
            }
        }
    }

    loop {
        report(70, "Extracting Debian...");
        if let Some(parent) = rootfs_path.parent() {
            let _ = std::fs::create_dir_all(parent);
        }
        let _ = std::fs::remove_dir_all(&temp_extract);
        let _ = std::fs::remove_dir_all(&staging_rootfs_path);

        match extract::extract_tarball(&tarball_path, &staging_rootfs_path, &temp_extract) {
            Ok(()) => {
                extract::validate_rootfs_structure(&staging_rootfs_path)
                    .context("validate extracted rootfs structure")?;
                break;
            }
            Err(e) => {
                log::warn!("debian rootfs extract failed, retry: {:?}", e);
                let _ = std::fs::remove_dir_all(&temp_extract);
                let _ = std::fs::remove_dir_all(&staging_rootfs_path);
            }
        }
    }

    let sentinel_path = staging_rootfs_path.join(ROOTFS_READY_SENTINEL);
    std::fs::write(&sentinel_path, b"").context("write rootfs ready sentinel")?;
    if !sentinel_path.exists() {
        anyhow::bail!("sentinel file not present after write");
    }

    let had_old = rootfs_path.exists();
    let mut backup_rootfs_path: Option<std::path::PathBuf> = None;
    if had_old {
        let ts = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis();
        let backup_name = format!("{}.bak.{}", rootfs_name, ts);
        let parent = rootfs_path
            .parent()
            .ok_or_else(|| anyhow::anyhow!("rootfs_path has no parent"))?;
        let backup_path = parent.join(backup_name);
        std::fs::rename(&rootfs_path, &backup_path).context("rename old rootfs -> backup")?;
        backup_rootfs_path = Some(backup_path);
    }

    match std::fs::rename(&staging_rootfs_path, &rootfs_path) {
        Ok(()) => {
            if let Some(ref backup_path) = backup_rootfs_path {
                let _ = std::fs::remove_dir_all(backup_path);
            }
        }
        Err(e) => {
            if let Some(ref backup_path) = backup_rootfs_path {
                let _ = std::fs::rename(backup_path, &rootfs_path)
                    .context("rollback backup rootfs -> debian")?;
            }
            return Err(e).context("atomic rootfs swap failed");
        }
    }

    let _ = std::fs::remove_file(&tarball_path);
    report(100, "Done");
    Ok(())
}

pub fn ensure_wine_rootfs_with_progress(progress: Option<ProgressFn>) -> Result<()> {
    let _guard = DOWNLOAD_LOCK
        .lock()
        .map_err(|e| anyhow::anyhow!("download lock poisoned: {:?}", e))?;
    let ctx = get_application_context()?;
    let rootfs_path = wine_rootfs_dir()?;
    let cache_dir = &ctx.cache_dir;

    if has_rootfs(&rootfs_path) {
        return Ok(());
    }

    let report = |pct: u32, msg: &str| {
        if let Some(ref f) = progress {
            f(pct, msg);
        }
    };

    std::fs::create_dir_all(cache_dir).context("create cache dir")?;
    let tarball_path = cache_dir.join(WINE_TARBALL_NAME);
    let tarball_tmp = cache_dir.join(format!("{}.tmp", WINE_TARBALL_NAME));
    let temp_extract = rootfs_path
        .parent()
        .map(|p| p.join("wine_extract_tmp"))
        .unwrap_or_else(|| rootfs_path.join("wine_extract_tmp"));
    let staging_rootfs_path = rootfs_path
        .parent()
        .map(|p| p.join("wine.new"))
        .unwrap_or_else(|| rootfs_path.with_extension("new"));
    let rootfs_name = rootfs_path
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("wine")
        .to_string();

    loop {
        if tarball_path.exists() {
            let existing = download::sha256_file(&tarball_path).context("sha256 existing tarball")?;
            if existing == WINE_TARBALL_SHA256 {
                break;
            }
            log::warn!("wine rootfs tarball sha256 mismatch, redownloading");
            let _ = std::fs::remove_file(&tarball_path);
        }

        report(0, "Downloading Wine...");
        let _ = std::fs::remove_file(&tarball_tmp);
        match download::download_tarball_with_progress(
            &tarball_tmp,
            &tarball_path,
            0,
            70,
            &report,
            WINE_TARBALL_URL,
            WINE_TARBALL_SHA256,
            "Downloading Wine",
        ) {
            Ok(()) => break,
            Err(e) => {
                log::warn!("wine rootfs download failed, retry: {:?}", e);
                let _ = std::fs::remove_file(&tarball_tmp);
                let _ = std::fs::remove_file(&tarball_path);
            }
        }
    }

    loop {
        report(70, "Extracting Wine...");
        if let Some(parent) = rootfs_path.parent() {
            let _ = std::fs::create_dir_all(parent);
        }
        let _ = std::fs::remove_dir_all(&temp_extract);
        let _ = std::fs::remove_dir_all(&staging_rootfs_path);

        match extract::extract_tarball(&tarball_path, &staging_rootfs_path, &temp_extract) {
            Ok(()) => {
                extract::validate_rootfs_structure(&staging_rootfs_path)
                    .context("validate extracted rootfs structure")?;
                break;
            }
            Err(e) => {
                log::warn!("wine rootfs extract failed, retry: {:?}", e);
                let _ = std::fs::remove_dir_all(&temp_extract);
                let _ = std::fs::remove_dir_all(&staging_rootfs_path);
            }
        }
    }

    let sentinel_path = staging_rootfs_path.join(ROOTFS_READY_SENTINEL);
    std::fs::write(&sentinel_path, b"").context("write rootfs ready sentinel")?;
    if !sentinel_path.exists() {
        anyhow::bail!("sentinel file not present after write");
    }

    let had_old = rootfs_path.exists();
    let mut backup_rootfs_path: Option<std::path::PathBuf> = None;
    if had_old {
        let ts = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis();
        let backup_name = format!("{}.bak.{}", rootfs_name, ts);
        let parent = rootfs_path
            .parent()
            .ok_or_else(|| anyhow::anyhow!("rootfs_path has no parent"))?;
        let backup_path = parent.join(backup_name);
        std::fs::rename(&rootfs_path, &backup_path).context("rename old rootfs -> backup")?;
        backup_rootfs_path = Some(backup_path);
    }

    match std::fs::rename(&staging_rootfs_path, &rootfs_path) {
        Ok(()) => {
            if let Some(ref backup_path) = backup_rootfs_path {
                let _ = std::fs::remove_dir_all(backup_path);
            }
        }
        Err(e) => {
            if let Some(ref backup_path) = backup_rootfs_path {
                let _ =
                    std::fs::rename(backup_path, &rootfs_path).context("rollback backup rootfs -> wine")?;
            }
            return Err(e).context("atomic rootfs swap failed");
        }
    }

    let _ = std::fs::remove_file(&tarball_path);
    report(100, "Done");
    Ok(())
}
