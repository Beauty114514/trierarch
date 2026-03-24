# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html) where applicable.

## [Unreleased]

### Added
- _(fill when you ship the next change)_

---

## [0.1.0] — 2026-03-21

### Added
- Initial public monorepo layout: Android app, Rust JNI, proot, Wayland compositor, optimize docs.

[Unreleased]: https://github.com/Beauty114514/trierarch/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/Beauty114514/trierarch/releases/tag/v0.1.0

## [0.2.0] - 2026-03-23

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