//! Rootfs acquisition for the Android app.
//!
//! Contract:
//! - Produces a usable Arch rootfs under the app data directory (e.g. `data_dir/arch`).
//! - Uses a cache tarball with SHA-256 verification.
//! - Extracts into a staging directory and performs an *atomic swap* into place.
//! - On failure, attempts to keep the previously working rootfs intact (rollback when possible).
//! - A sentinel file (`ROOTFS_READY_SENTINEL`) is written into the staging rootfs to mark completeness.
//!
//! This module is Trierarch-owned logic (not vendored) and should keep comments focused on
//! invariants, failure semantics, and on-disk state transitions.

use super::application_context::{
    get_application_context, has_rootfs, rootfs_dir, ROOTFS_READY_SENTINEL,
};
use anyhow::{Context, Result};
use std::sync::Mutex;
use std::time::{SystemTime, UNIX_EPOCH};

mod download;
mod extract;

static DOWNLOAD_LOCK: Mutex<()> = Mutex::new(());
const TARBALL_NAME: &str = "archlinux-aarch64-pd-v4.34.2.tar.xz";
const TARBALL_SHA256: &str = "dabc2382ddcb725969cf7b9e2f3b102ec862ea6e0294198a30c71e9a4b837f81";

pub type ProgressFn = Box<dyn Fn(u32, &str) + Send>;

pub fn ensure_arch_rootfs_with_progress(progress: Option<ProgressFn>) -> Result<()> {
    let _guard = DOWNLOAD_LOCK
        .lock()
        .map_err(|e| anyhow::anyhow!("download lock poisoned: {:?}", e))?;
    let ctx = get_application_context()?;
    let rootfs_path = rootfs_dir()?;
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
    let tarball_path = cache_dir.join(TARBALL_NAME);
    let tarball_tmp = cache_dir.join(format!("{}.tmp", TARBALL_NAME));
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
            if existing == TARBALL_SHA256 {
                break;
            }
            log::warn!(
                "Existing rootfs tarball sha256 mismatch, redownloading: expected={}, got={}",
                TARBALL_SHA256,
                existing
            );
            let _ = std::fs::remove_file(&tarball_path);
        }

        report(0, "Downloading Arch Linux FS...");
        let _ = std::fs::remove_file(&tarball_tmp);
        match download::download_tarball_with_progress(&tarball_tmp, &tarball_path, 0, 70, &report)
        {
            Ok(()) => break,
            Err(e) => {
                log::warn!("Download failed, retrying: {:?}", e);
                let _ = std::fs::remove_file(&tarball_tmp);
                let _ = std::fs::remove_file(&tarball_path);
            }
        }
    }

    loop {
        report(70, "Extracting Arch Linux FS...");
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
                log::warn!("Extract failed: {:?}. Retrying extract (tarball kept).", e);
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
