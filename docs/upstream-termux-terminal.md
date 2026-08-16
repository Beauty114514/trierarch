# Termux terminal source tracking

## Upstream baseline

- Repository: https://github.com/termux/termux-app
- Commit: `3df69d1da197dd9bd71a3bafd902dffd720576b4`
- Commit date: 2026-07-15

The following upstream source packages are intended to be compiled as part of Trierarch App:

- `com.termux.terminal`
- `com.termux.view`

Their original package names are retained. Trierarch-owned Android code remains under
`app.trierarch`.

## Runtime boundary

Trierarch does not create `com.termux.terminal.TerminalSession` and does not use Termux's PTY
native implementation (`termux.c` / `libtermux.so`). Process creation, PTY I/O, and resizing are
owned by Trierarch native code.

Termux terminal code is used for terminal emulation, rendering, input handling, scrolling, and
text selection. The native PTY session is connected to the terminal view through a small
Trierarch-maintained adapter based on the old project's `DisplayableTermSession` design.

## License

The Termux repository is GPLv3-only overall. Its `terminal-emulator` and `terminal-view` modules
are documented by Termux as code derived from Terminal Emulator for Android under the Apache 2.0
license exception. Preserve upstream copyright and license notices when importing or updating
these modules.

## Update rule

Never replace these sources from a floating branch. Each import must record an exact upstream
commit here and keep Trierarch-specific changes limited to the terminal-view/session adapter.

## Trierarch patch set

The complete source-level difference from the baseline is stored in
[`patches/termux-terminal/`](../patches/termux-terminal/), in this order:

1. `0001-add-displayable-term-session.patch`
2. `0002-adapt-terminal-view-session.patch`
3. `0003-adapt-terminal-view-client.patch`
4. `0004-use-app-resource-namespace.patch`

After importing a new upstream snapshot into the same paths, apply the compatible patches from
the `trierarch-app` repository root:

```sh
for patch_file in patches/termux-terminal/*.patch; do
  patch -p1 < "$patch_file"
done
```

The fourth patch is required because these sources are compiled in the `app.trierarch` Android
application namespace rather than Termux's separate `com.termux.view` library namespace.
