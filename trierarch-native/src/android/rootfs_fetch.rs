// Fetch Arch Linux rootfs from proot-distro when not present.

use super::application_context::{
    get_application_context, has_rootfs, rootfs_dir, ROOTFS_READY_SENTINEL,
};
use anyhow::{Context, Result};
use sha2::{Digest, Sha256};
use std::fs::File;
use std::io::{BufReader, Read, Write};
use std::path::Path;
use std::sync::Mutex;
use std::time::{SystemTime, UNIX_EPOCH};

static DOWNLOAD_LOCK: Mutex<()> = Mutex::new(());
const TARBALL_NAME: &str = "archlinux-aarch64-pd-v4.34.2.tar.xz";
const TARBALL_URL: &str =
    "https://github.com/termux/proot-distro/releases/download/v4.34.2/archlinux-aarch64-pd-v4.34.2.tar.xz";
const TARBALL_SHA256: &str = "dabc2382ddcb725969cf7b9e2f3b102ec862ea6e0294198a30c71e9a4b837f81";

/// Progress callback: (percent 0-100, message).
pub type ProgressFn = Box<dyn Fn(u32, &str) + Send>;

/// Fetch and extract Arch rootfs to data_dir/arch if not present. Reports progress via callback.
/// Retries download and extraction until success; cleans up partial files on failure.
/// Only one thread may run this at a time (guard against duplicate Kotlin launches).
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

    // Step 1: Ensure tarball matches the pinned SHA256.
    // If the tarball exists but has the wrong hash (e.g. older cache, partial download, or tampering),
    // delete it and retry the download.
    loop {
        if tarball_path.exists() {
            let existing = sha256_file(&tarball_path).context("sha256 existing tarball")?;
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
        match download_tarball_with_progress(&tarball_tmp, &tarball_path, 0, 70, &report) {
            Ok(()) => break,
            Err(e) => {
                log::warn!("Download failed, retrying: {:?}", e);
                let _ = std::fs::remove_file(&tarball_tmp);
                let _ = std::fs::remove_file(&tarball_path);
            }
        }
    }

    // Step 2: Extract. On failure only clear extract dirs and retry; never delete tarball.
    loop {
        report(70, "Extracting Arch Linux FS...");
        if let Some(parent) = rootfs_path.parent() {
            let _ = std::fs::create_dir_all(parent);
        }
        let _ = std::fs::remove_dir_all(&temp_extract);
        let _ = std::fs::remove_dir_all(&staging_rootfs_path);

        match extract_tarball(&tarball_path, &staging_rootfs_path, &temp_extract) {
            Ok(()) => {
                // Only mark the rootfs as ready after verifying the minimum expected structure.
                // This prevents "successful extract but incomplete rootfs" cases from becoming
                // a permanent broken state via the sentinel file.
                validate_rootfs_structure(&staging_rootfs_path)
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

    // Step 3: Mark rootfs ready then atomically swap.
    // Do not delete/overwrite the old rootfs until we've validated the new one.
    let sentinel_path = staging_rootfs_path.join(ROOTFS_READY_SENTINEL);
    std::fs::write(&sentinel_path, b"").context("write rootfs ready sentinel")?;
    if !sentinel_path.exists() {
        anyhow::bail!("sentinel file not present after write");
    }

    // Atomic swap:
    // - arch/ -> arch.bak.<ts> (unique, so we avoid rename/delete permission issues)
    // - arch.new -> arch
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
            // Best-effort cleanup; if permission denies, we still keep the backup.
            if let Some(ref backup_path) = backup_rootfs_path {
                let _ = std::fs::remove_dir_all(backup_path);
            }
        }
        Err(e) => {
            // Roll back old rootfs so the app doesn't get stuck in a half-installed state.
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

fn download_tarball_with_progress<F>(
    tmp_path: &Path,
    final_path: &Path,
    pct_min: u32,
    pct_max: u32,
    report: &F,
) -> Result<()>
where
    F: Fn(u32, &str),
{
    log::info!("Downloading Arch rootfs from proot-distro ({} MB)...", 156);

    let client = reqwest::blocking::Client::builder()
        .timeout(std::time::Duration::from_secs(3600))
        .build()
        .context("create HTTP client")?;

    let mut response = client.get(TARBALL_URL).send().context("download request")?;

    if !response.status().is_success() {
        anyhow::bail!(
            "download failed: {} {}",
            response.status(),
            response.text().unwrap_or_default()
        );
    }

    let total = response.content_length().unwrap_or(0);
    let mut file = File::create(tmp_path).context("create temp file")?;
    let mut downloaded: u64 = 0;
    let mut buf = [0u8; 8192]; // Local Desktop uses 8KB chunks
    let mut last_reported_pct = 0u8;
    let mut hasher = Sha256::new();

    loop {
        let n = response.read(&mut buf).context("read response")?;
        if n == 0 {
            break;
        }
        file.write_all(&buf[..n]).context("write file")?;
        hasher.update(&buf[..n]);
        downloaded += n as u64;
        if total > 0 {
            let percent = (downloaded * 100 / total).min(100) as u8;
            if percent != last_reported_pct {
                let downloaded_mb = downloaded as f64 / 1024.0 / 1024.0;
                let total_mb = total as f64 / 1024.0 / 1024.0;
                let msg = format!(
                    "Downloading Arch Linux FS... {}% ({:.2} MB / {:.2} MB)",
                    percent, downloaded_mb, total_mb
                );
                let pct = pct_min + ((percent as u32) * (pct_max - pct_min) / 100);
                report(pct, &msg);
                last_reported_pct = percent;
            }
        }
    }

    report(pct_max, "Downloading Arch Linux FS... 100%");
    file.flush().context("flush temp file")?;
    drop(file);

    // Pin the downloaded tarball content to reduce the chance of corrupted/tampered cache.
    let actual = format!("{:x}", hasher.finalize());
    if actual != TARBALL_SHA256 {
        let _ = std::fs::remove_file(tmp_path);
        anyhow::bail!(
            "tarball sha256 mismatch: expected={}, got={}",
            TARBALL_SHA256,
            actual
        );
    }

    if std::fs::rename(tmp_path, final_path).is_err() {
        std::fs::copy(tmp_path, final_path).context("copy temp to final")?;
        let _ = std::fs::remove_file(tmp_path);
    }
    log::info!("Download complete.");
    Ok(())
}

fn sha256_file(path: &Path) -> Result<String> {
    let file = File::open(path).context("open file for sha256")?;
    let mut reader = BufReader::new(file);
    let mut hasher = Sha256::new();
    let mut buf = [0u8; 8192];
    loop {
        let n = reader.read(&mut buf).context("read file for sha256")?;
        if n == 0 {
            break;
        }
        hasher.update(&buf[..n]);
    }
    Ok(format!("{:x}", hasher.finalize()))
}

fn validate_rootfs_structure(rootfs_path: &Path) -> Result<()> {
    // Minimal expected userland shell.
    let has_sh = rootfs_path.join("bin/sh").exists() || rootfs_path.join("usr/bin/sh").exists();
    anyhow::ensure!(has_sh, "missing bin/sh or usr/bin/sh");

    // Basic OS metadata.
    anyhow::ensure!(
        rootfs_path.join("etc/os-release").exists(),
        "missing etc/os-release"
    );

    // Termux-style simulated data for proot on Android (created during setup_fake_sysdata()).
    anyhow::ensure!(rootfs_path.join("proc").is_dir(), "missing proc/");
    anyhow::ensure!(
        rootfs_path.join("sys/.empty").is_dir(),
        "missing sys/.empty"
    );
    anyhow::ensure!(
        rootfs_path.join("proc/.loadavg").is_file(),
        "missing proc/.loadavg"
    );
    anyhow::ensure!(
        rootfs_path.join("proc/.stat").is_file(),
        "missing proc/.stat"
    );

    Ok(())
}

fn extract_tarball(tarball_path: &Path, dest: &Path, temp_extract: &Path) -> Result<()> {
    log::info!("Extracting Arch rootfs...");

    let file = File::open(tarball_path).context("open tarball")?;
    let xz_decoder = xz2::read::XzDecoder::new(BufReader::new(file));

    std::fs::create_dir_all(temp_extract).context("create temp extract dir")?;

    let mut archive = tar::Archive::new(xz_decoder);
    archive.unpack(temp_extract).context("extract tarball")?;

    // proot-distro tarball has one top-level dir; move it to dest
    let subdirs: Vec<_> = std::fs::read_dir(temp_extract)
        .context("list temp dir")?
        .filter_map(|e| e.ok())
        .collect();

    if subdirs.len() != 1 {
        anyhow::bail!(
            "tarball has {} top-level entries, expected 1",
            subdirs.len()
        );
    }
    let top = subdirs[0].path();
    if !top.is_dir() {
        anyhow::bail!("tarball top-level is not a directory");
    }
    let _ = std::fs::remove_dir_all(dest);
    std::fs::rename(&top, dest).context("rename rootfs to dest")?;

    std::fs::remove_dir_all(temp_extract).ok();
    log::info!("Extraction complete.");

    // Termux-style: create fake /proc and /sys entries for proot (Android restricts real ones).
    setup_fake_sysdata(dest)?;

    Ok(())
}

/// Create fake proc/sys files for proot (matches proot-distro setup_fake_sysdata).
/// Android restricts reading real /proc and /sys; proot binds these fakes instead.
fn setup_fake_sysdata(rootfs: &Path) -> Result<()> {
    let proc_dir = rootfs.join("proc");
    let sys_empty = rootfs.join("sys/.empty");
    std::fs::create_dir_all(&proc_dir).context("create proc dir")?;
    std::fs::create_dir_all(&sys_empty).context("create sys/.empty")?;

    let write_if_missing = |path: &Path, content: &str| {
        if !path.exists() {
            std::fs::write(path, content).with_context(|| format!("write {:?}", path))?;
        }
        Ok::<(), anyhow::Error>(())
    };

    // Match LocalDesktop simulate_linux_sysdata_stage exactly for identical Arch environment
    write_if_missing(&rootfs.join("proc/.loadavg"), "0.12 0.07 0.02 2/165 765\n")?;
    write_if_missing(
        &rootfs.join("proc/.stat"),
        "cpu 1957 0 2877 93280 262 342 254 87 0 0\ncpu0 31 0 226 12027 82 10 4 9 0 0\n",
    )?;
    write_if_missing(&rootfs.join("proc/.uptime"), "124.08 932.80\n")?;
    write_if_missing(
        &rootfs.join("proc/.version"),
        "Linux version 6.2.1 (proot@termux) (gcc (GCC) 12.2.1 20230201, GNU ld (GNU Binutils) 2.40) #1 SMP PREEMPT_DYNAMIC Wed, 01 Mar 2023 00:00:00 +0000\n",
    )?;
    write_if_missing(
        &rootfs.join("proc/.vmstat"),
        "nr_free_pages 1743136\nnr_zone_inactive_anon 179281\nnr_zone_active_anon 7183\n",
    )?;
    write_if_missing(&rootfs.join("proc/.sysctl_entry_cap_last_cap"), "40\n")?;
    write_if_missing(
        &rootfs.join("proc/.sysctl_inotify_max_user_watches"),
        "4096\n",
    )?;

    log::info!("setup_fake_sysdata done.");
    Ok(())
}
