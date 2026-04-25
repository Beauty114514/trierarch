//! Spawn and supervise the proot shell under a PTY.
//!
//! Contract:
//! - Spawns an interactive shell inside the installed rootfs using `forkpty` + `execve`.
//! - Each call creates one independent proot+PTY+shell (one "session"). Output is delivered with a session id.
//! - Returns handles for the parent: child PID (for supervision), PTY read end, PTY write end, master fd for ioctl.

use anyhow::{Context, Result};
use nix::pty::{forkpty, ForkptyResult, Winsize};
use nix::unistd::{dup, execve, Pid};
use std::io::Write;
use std::os::fd::IntoRawFd;
use std::os::unix::io::{FromRawFd, RawFd};

mod args;

pub struct ChildProcess {
    pub pid: Pid,
}

impl Drop for ChildProcess {
    fn drop(&mut self) {
        let _ = nix::sys::signal::kill(self.pid, nix::sys::signal::Signal::SIGTERM);
    }
}

/// One interactive proot shell on a new PTY. Does not register JNI state — caller owns read/write/fd lifecycle.
pub fn fork_pty_shell(
    initial_rows: u16,
    initial_cols: u16,
) -> Result<(ChildProcess, std::fs::File, Box<dyn Write + Send>, RawFd)> {
    let rootfs = super::application_context::arch_rootfs_dir()?;
    fork_pty_shell_in_rootfs(&rootfs, initial_rows, initial_cols)
}

pub fn fork_pty_shell_in_rootfs(
    rootfs: &std::path::Path,
    initial_rows: u16,
    initial_cols: u16,
) -> Result<(ChildProcess, std::fs::File, Box<dyn Write + Send>, RawFd)> {
    let (argv, env) = args::build_exec_args(rootfs)?;

    let argv_refs: Vec<&std::ffi::CStr> = argv.iter().map(|s| s.as_c_str()).collect();
    let env_refs: Vec<&std::ffi::CStr> = env.iter().map(|s| s.as_c_str()).collect();

    let winsize = Winsize {
        ws_row: initial_rows.max(1),
        ws_col: initial_cols.max(1),
        ws_xpixel: 0,
        ws_ypixel: 0,
    };
    let result = unsafe { forkpty(Some(&winsize), None).context("forkpty failed")? };

    match result {
        ForkptyResult::Child => {
            if execve(argv[0].as_c_str(), &argv_refs, &env_refs).is_err() {
                unsafe { nix::libc::_exit(1) };
            }
            unreachable!();
        }
        ForkptyResult::Parent { child, master } => {
            let master_read_fd = dup(&master).context("dup master for read")?.into_raw_fd();
            let master_write_fd = master.into_raw_fd();
            let master_read = unsafe { std::fs::File::from_raw_fd(master_read_fd) };
            let master_write = unsafe { std::fs::File::from_raw_fd(master_write_fd) };
            let stdin: Box<dyn Write + Send> = Box::new(master_write);
            Ok((
                ChildProcess { pid: child },
                master_read,
                stdin,
                master_write_fd,
            ))
        }
    }
}
