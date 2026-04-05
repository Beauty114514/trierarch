# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html) where applicable.

## [Unreleased]

## [0.3.0] — 2026-04-04

### Added
- In-app **terminal emulator** (Termux `TerminalView`) for the proot shell, with bundled **monospace fonts**, **Appearance** selection, and **`assets/licenses/FONT_LICENSES.txt`** (SIL OFL 1.1 and Apache 2.0).
- **Multi-session terminal:** native **per-session PTY** (spawn/close, per-session stdin and PTY output relay) with Kotlin **Session** UI (orb menu → list, switch, add, remove tabs). Terminal sessions use ids **1, 2, …**.
- **Display session separation:** **Display startup script** is injected on **session 0** (dedicated shell), independent of interactive terminal sessions, so desktop bootstrap and terminal tabs no longer share one stdin/PTY.

### Changed
- **Terminal** screen respects **status bar** and **soft keyboard** insets.
- **UI codebase** reorganized into **orb**, **glass**, **dialogs**, **shell**, and **setup** modules, with matching **native** updates.
- **Pointer sensitivity:** separate **scroll axis scaling** for physical wheel vs two-finger gestures in the compositor, and **faster simulated touchpad cursor** movement (finger delta × 2.5) in touchpad mode.
- **Adaptive glass menus:** the orb menu, settings sheets (view, appearance, sessions), and the display script editor now **size from the host window** via shared glass layout (`BoxWithConstraints`, width caps, and edge insets) so panels stay centered and within the screen instead of fixed dialog geometry; nested pickers use the same overlay stack.
- **Floating menu orb:** grouped items under **Common**, **Desktop**, and **Terminal** section labels; slightly larger typography for those section headers and matching settings subtitles (e.g. View settings, Appearance).

### Fixed
- **Touchpad mode (Wayland):** two-finger gestures no longer leave a **delayed left-press** (hold-drag runnable was not canceled when the second finger was handled elsewhere) or **inconsistent state** after a two-finger tap → right click (final `ACTION_UP` consumed without syncing the simulated touchpad controller).

### Removed
- Older **placeholder terminal** UI and superseded **monolithic** glass/menu sources.

---

## [0.2.3] — 2026-03-31

### Changed
- **Display startup script** editor (long-press **Display** on the orb menu) now uses the same glass panel and scrim as view settings.
- **View settings** now opens compact glass single-choice dialogs for mouse mode, resolution, and scale instead of inline dropdowns.

### Fixed
- Soft keyboard now **auto-restores** after switching between keyboard apps (both desktop and terminal).
- Avoid the **double cursor** effect when a physical mouse is active by hiding the Wayland cursor.
- Fixed physical mouse pointer **position desync** after output size/scale changes by keeping Surface size in sync with coordinate mapping.
- Cursor is now rendered at a **stable physical pixel size** (not affected by resolution/scale), and hotspot scaling avoids visible jumps.
- Splash background bitmap filtering disabled to reduce **flash/blur** during launch scaling.
- Prevent rare **black-screen** cases when frantically tapping during KDE startup by delaying entry into the Wayland view until a desktop client is present.

## [0.2.2] — 2026-03-29

### Added
- Floating **menu orb**: draggable glass-style control with a dropdown menu; remembers position between sessions (replaces the side slide-out menu).

### Changed
- **View settings** uses the same glass surface and motion language as the menu; dialog sizing avoids stacked dimming so the backdrop is not overly dark.
- Menu and view-settings titles use stronger emphasis (e.g. semi-bold **Menu** / **View settings**) with clearer label colors for actions.

### Removed
- Side **slide-out menu** (`SideMenu`); all primary navigation actions live on the floating orb menu.

## [0.2.1] — 2026-03-27

### Added
- Added a launcher app shortcut: **Terminal** (long-press app icon → Terminal) to jump directly into the proot shell.

### Changed
- Default launch now auto-enters the desktop view and automatically injects/runs the configured **Display startup script** (idempotent when a desktop client is already connected).
- Wayland server is now started by default (removed the explicit Wayland toggle from the side menu).
- Updated README flow and screenshots to match the new shortcut + auto-start behavior.
- Standardized internal code comments and removed external-project references.

## [0.2.0] — 2026-03-23

### Added
- Added physical keyboard and mouse input support path while keeping existing soft keyboard and simulated mouse flow.
- Added keycode mapping coverage for function keys (F1-F24), Fn, CapsLock, NumLock, and ScrollLock.
- Added CI workflow checks for native Rust and Android Kotlin compilation.
- Added input regression checklist documentation.

### Changed
- Refactored input handling to clearer modules (hardware keyboard routing, soft keyboard view, Wayland surface integration).
- Updated Wayland source management to on-demand upstream fetch into `trierarch-wayland/build-src/` (not tracked in git).

### Fixed
- Fixed sticky modifier behavior (Shift/Ctrl/Alt/Meta/Fn) by replacing fragile counter-based state with explicit pressed-state tracking in Wayland keyboard handling.
- Fixed lock-key state path for CapsLock/NumLock/ScrollLock to prevent desync.
- Improved shortcut routing by mapping `Alt+CapsLock` to in-app window switch behavior (workaround for Android-level `Alt+Tab` interception limits).

### Known Issues
- Physical `Alt+Tab` may still be intercepted by Android system and cannot always be fully overridden at app level.

## [0.1.0] — 2026-03-21

### Added
- Initial public monorepo layout: Android app, Rust JNI, proot, Wayland compositor, optimize docs.

[Unreleased]: https://github.com/Beauty114514/trierarch/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/Beauty114514/trierarch/compare/v0.2.3...v0.3.0
[0.2.3]: https://github.com/Beauty114514/trierarch/compare/v0.2.2...v0.2.3
[0.2.2]: https://github.com/Beauty114514/trierarch/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/Beauty114514/trierarch/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/Beauty114514/trierarch/releases/tag/v0.2.0
[0.1.0]: https://github.com/Beauty114514/trierarch/releases/tag/v0.1.0