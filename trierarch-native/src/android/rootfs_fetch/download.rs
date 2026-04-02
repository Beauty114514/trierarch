//! Download helpers for the Arch rootfs tarball.
//!
//! Contract:
//! - Writes to `tmp_path` while streaming, computing SHA-256 on the fly.
//! - Verifies the digest before moving the file into `final_path`.
//! - Uses a best-effort atomic move; falls back to copy+delete on filesystems where rename fails.
//! - Progress reporting is intentionally coarse (percent changes) to keep overhead low.

use anyhow::{Context, Result};
use sha2::{Digest, Sha256};
use std::fs::File;
use std::io::{BufReader, Read, Write};
use std::path::Path;

const TARBALL_URL: &str =
    "https://github.com/termux/proot-distro/releases/download/v4.34.2/archlinux-aarch64-pd-v4.34.2.tar.xz";
const TARBALL_SHA256: &str = "dabc2382ddcb725969cf7b9e2f3b102ec862ea6e0294198a30c71e9a4b837f81";

pub(super) fn download_tarball_with_progress<F>(
    tmp_path: &Path,
    final_path: &Path,
    pct_min: u32,
    pct_max: u32,
    report: &F,
) -> Result<()>
where
    F: Fn(u32, &str),
{
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
    let mut buf = [0u8; 8192];
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
    Ok(())
}

pub(super) fn sha256_file(path: &Path) -> Result<String> {
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
