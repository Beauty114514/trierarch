# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html) where applicable.

## [Unreleased]

### Added
- _(fill when you ship the next change)_

### Changed
- **Display startup script** editor (long-press **Display** on the orb menu) now uses the same glass panel and scrim as view settings.

### Fixed
- Soft keyboard now **auto-restores** after switching between keyboard apps (both desktop and terminal).
- Avoid the **double cursor** effect when a physical mouse is active by hiding the Wayland cursor.
- Fixed physical mouse pointer **position desync** after output size/scale changes by keeping Surface size in sync with coordinate mapping.
- Cursor is now rendered at a **stable physical pixel size** (not affected by resolution/scale), and hotspot scaling avoids visible jumps.
- Splash background bitmap filtering disabled to reduce **flash/blur** during launch scaling.

---

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

[Unreleased]: https://github.com/Beauty114514/trierarch/compare/v0.2.2...HEAD
[0.2.2]: https://github.com/Beauty114514/trierarch/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/Beauty114514/trierarch/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/Beauty114514/trierarch/releases/tag/v0.2.0
[0.1.0]: https://github.com/Beauty114514/trierarch/releases/tag/v0.1.0