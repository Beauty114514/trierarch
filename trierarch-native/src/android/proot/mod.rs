//! Spawn and supervise the proot shell under a PTY.
//!
//! Contract:
//! - Spawns an interactive shell inside the installed rootfs using `forkpty` + `execve`.
//! - Streams raw PTY master output to the UI via JNI (Termux TerminalEmulator).
//! - Returns a `Write` handle for PTY stdin; dropping the returned `ChildProcess` will SIGTERM the child.

use anyhow::{Context, Result};
use nix::pty::{forkpty, ForkptyResult, Winsize};
use nix::unistd::{dup, execve, Pid};
use std::io::Write;
use std::os::fd::IntoRawFd;
use std::os::unix::io::FromRawFd;

mod args;

pub struct ChildProcess {
    pub pid: Pid,
}

impl Drop for ChildProcess {
    fn drop(&mut self) {
        let _ = nix::sys::signal::kill(self.pid, nix::sys::signal::Signal::SIGTERM);
    }
}

pub fn spawn_sh_with_pty_reader(
    initial_rows: u16,
    initial_cols: u16,
) -> Result<(ChildProcess, Option<Box<dyn Write + Send>>)> {
    let rootfs = super::application_context::rootfs_dir()?;
    let (argv, env) = args::build_exec_args(&rootfs)?;

    let argv_refs: Vec<&std::ffi::CStr> = argv.iter().map(|s| s.as_c_str()).collect();
    let env_refs: Vec<&std::ffi::CStr> = env.iter().map(|s| s.as_c_str()).collect();

    // Must match the first Java [TerminalView] grid; a 80×24 PTY here causes bash to paint a prompt
    // before TIOCSWINSZ, then SIGWINCH redraws and can leave two PS1 fragments on one line.
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

            crate::jni_context::register_pty_master_fd(master_write_fd)?;
            std::thread::spawn(move || {
                crate::jni_context::pty_master_reader_loop(master_read);
            });

            Ok((ChildProcess { pid: child }, Some(Box::new(master_write))))
        }
    }
}
