//! Tar extract, placeholder proc/sys, structure checks.

use anyhow::{Context, Result};
use std::fs::File;
use std::io::BufReader;
use std::path::Path;

pub(super) fn validate_rootfs_structure(rootfs_path: &Path) -> Result<()> {
    let has_sh = rootfs_path.join("bin/sh").exists() || rootfs_path.join("usr/bin/sh").exists();
    anyhow::ensure!(has_sh, "missing bin/sh or usr/bin/sh");
    anyhow::ensure!(
        rootfs_path.join("etc/os-release").exists(),
        "missing etc/os-release"
    );
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

pub(super) fn extract_tarball(tarball_path: &Path, dest: &Path, temp_extract: &Path) -> Result<()> {
    let file = File::open(tarball_path).context("open tarball")?;
    let xz_decoder = xz2::read::XzDecoder::new(BufReader::new(file));

    std::fs::create_dir_all(temp_extract).context("create temp extract dir")?;

    let mut archive = tar::Archive::new(xz_decoder);
    archive.unpack(temp_extract).context("extract tarball")?;

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
    setup_fake_sysdata(dest)?;
    Ok(())
}

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

    Ok(())
}
