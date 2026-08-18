# Termux:X11 Lorie integration

## Native baseline

- Repository: https://github.com/termux/termux-x11
- Commit: `9471ad977d21b7c7bec45117008f74f330d45983`
- Native source and Android build recipe: the separate
  `trierarch-packages/x11-host` repository.

The application packages only that repository's `libXlorie.so` and its XKB
rules asset. The package is built independently before an app build.

## Deliberate first boundary

`com.termux.x11` contains a tiny JNI-ABI bridge, not a copy of Termux:X11's
Android UI. Lorie requires those exact Java class and method names. The server
runs in the app's `:x11` process because its native Xorg entry point exits the
owning process when the server ends. A Binder transfers the per-client Unix
socket fd to the main-process `LorieView`.

The first integration supplies rendering. Input, clipboard, output selection,
and scaling policy remain separate configurable features.

## DroidSpaces session start

Trierarch owns the start decision for a DroidSpaces profile while DroidSpaces
remains the runtime that creates namespaces and starts init. When the container
is stopped, Trierarch first starts Lorie, then invokes DroidSpaces `start` with
the app X11 directory bound at `/tmp/.X11-unix`. Only after startup does it run
the configured command with `DISPLAY=:0` and `XDG_SESSION_TYPE=x11`.

`droidspaces run` only enters an already-running container and cannot add a
bind mount. Consequently, Trierarch refuses to launch an X11 profile into an
already-running container that was not started by that profile. It never
silently restarts or reconfigures such a container.

Profiles may optionally declare a direct program launch:

```toml
[launch]
argv = ["startxfce4"]
```

With no `launch.argv`, Trierarch opens the configured user's interactive
shell. `argv` is deliberately an array, so the application does not invent a
shell parser or mutate user shell startup files.

The currently pinned Lorie artifact is built with Android API 26 as its
minimum supported platform. A profile requesting `display.type = "x11"` is
therefore rejected cleanly on Android versions below 8.0; terminal-only
profiles retain the app's lower baseline.
