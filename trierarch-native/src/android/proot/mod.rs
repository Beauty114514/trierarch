//! Spawn and supervise the proot shell under a PTY.
//!
//! Contract:
//! - Spawns an interactive shell inside the installed rootfs using `forkpty` + `execve`.
//! - Starts a background reader thread that streams PTY output into line buffers for the UI.
//! - Returns a `Write` handle for PTY stdin; dropping the returned `ChildProcess` will SIGTERM the child.
//!
//! Threading:
//! - The PTY reader runs on a dedicated Rust thread. UI polls buffered output via JNI.
//! - Callers must not attempt to read from the PTY master directly; only via the provided buffers.
//!
use anyhow::{Context, Result};
use nix::pty::{forkpty, ForkptyResult, Winsize};
use nix::unistd::{dup, execve, Pid};
use std::collections::VecDeque;
use std::io::Write;
use std::os::fd::IntoRawFd;
use std::os::unix::io::FromRawFd;
use std::sync::Arc;

mod args;
mod pty_output;

pub struct ChildProcess {
    pub pid: Pid,
}

impl Drop for ChildProcess {
    fn drop(&mut self) {
        let _ = nix::sys::signal::kill(self.pid, nix::sys::signal::Signal::SIGTERM);
    }
}

pub fn spawn_sh_with_output(
    lines: Arc<std::sync::Mutex<VecDeque<String>>>,
    partial_line: Arc<std::sync::Mutex<String>>,
) -> Result<(ChildProcess, Option<Box<dyn Write + Send>>)> {
    let rootfs = super::application_context::rootfs_dir()?;
    let (argv, env) = args::build_exec_args(&rootfs)?;

    let argv_refs: Vec<&std::ffi::CStr> = argv.iter().map(|s| s.as_c_str()).collect();
    let env_refs: Vec<&std::ffi::CStr> = env.iter().map(|s| s.as_c_str()).collect();

    let winsize = Winsize {
        ws_row: 24,
        ws_col: 80,
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

            let lines_out = Arc::clone(&lines);
            let partial_out = Arc::clone(&partial_line);
            std::thread::spawn(move || pty_output::read_pty_to_lines(master_read, lines_out, partial_out));

            Ok((ChildProcess { pid: child }, Some(Box::new(master_write))))
        }
    }
}
