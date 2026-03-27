use std::collections::VecDeque;
use std::fs::File;
use std::io::Read;
use std::sync::Arc;

//! PTY output buffering for the terminal UI.
//!
//! Contract:
//! - Reads bytes from the PTY master and maintains:
//!   - a bounded deque of completed lines (`lines`)
//!   - the current in-progress line (`partial_line`)
//! - Strips/ignores ANSI escape sequences (CSI/OSC) so the Compose terminal can render plain text.
//!
//! Performance/safety:
//! - `MAX_LINES` bounds memory use; the UI is a live terminal, not a full scrollback archive.
//! - The parser is intentionally minimal: it aims to keep prompts readable, not to emulate a full terminal.

#[derive(Clone, Copy)]
enum AnsiState {
    Normal,
    Escape,
    Csi,
    Osc,
    OscEsc,
}

const MAX_LINES: usize = 5000;

pub(super) fn read_pty_to_lines(
    mut master: File,
    lines: Arc<std::sync::Mutex<VecDeque<String>>>,
    partial_line: Arc<std::sync::Mutex<String>>,
) {
    let mut buf = [0u8; 4096];
    let mut line = String::new();
    let mut ansi = AnsiState::Normal;
    loop {
        match master.read(&mut buf) {
            Ok(0) => break,
            Ok(n) => {
                for &b in &buf[..n] {
                    match ansi {
                        AnsiState::Escape => {
                            ansi = match b {
                                b'[' => AnsiState::Csi,
                                b']' => AnsiState::Osc,
                                _ if (0x40..=0x7e).contains(&b) => AnsiState::Normal,
                                _ => AnsiState::Escape,
                            };
                        }
                        AnsiState::Csi => {
                            if (0x40..=0x7e).contains(&b) {
                                ansi = AnsiState::Normal;
                            }
                        }
                        AnsiState::Osc => {
                            if b == 0x07 {
                                ansi = AnsiState::Normal;
                            } else if b == 0x1b {
                                ansi = AnsiState::OscEsc;
                            }
                        }
                        AnsiState::OscEsc => {
                            ansi = if b == b'\\' { AnsiState::Normal } else { AnsiState::Osc };
                        }
                        AnsiState::Normal => match b {
                            b'\n' | b'\r' => {
                                let l = std::mem::take(&mut line);
                                if !l.is_empty() {
                                    if let Ok(mut v) = lines.lock() {
                                        v.push_back(l);
                                        while v.len() > MAX_LINES {
                                            v.pop_front();
                                        }
                                    }
                                }
                                let _ = partial_line.lock().map(|mut p| *p = String::new());
                            }
                            0x1b => ansi = AnsiState::Escape,
                            _ => {
                                if b >= 0x20 || b == b'\t' {
                                    line.push(b as char);
                                    let _ = partial_line.lock().map(|mut p| *p = line.clone());
                                }
                            }
                        },
                    }
                }
            }
            Err(_) => break,
        }
    }
    let l = std::mem::take(&mut line);
    if !l.is_empty() {
        if let Ok(mut v) = lines.lock() {
            v.push_back(l);
            while v.len() > MAX_LINES {
                v.pop_front();
            }
        }
    }
}
