# Contributing to Trierarch

[中文](CONTRIBUTING.zh.md) | English

Thanks for your interest in this project. This document is a **starting point**—adjustments are welcome via PR.

## Communication

- **Issues and PRs** can be in **English or 中文** (or mixed). Prefer the language you are comfortable with.
- For **large changes** (new features, refactors, API changes), open an **issue first** so we can align on direction.

## How to contribute

1. **Fork** the repository and create a branch from **`main`**.
2. Make focused commits with **clear messages** (e.g. `fix(native): …`, `docs: …`, `feat(app): …`—[Conventional Commits](https://www.conventionalcommits.org/) style is appreciated but not mandatory).
3. Open a **Pull Request** against `main` with a short description of **what** and **why**.
4. If your change touches **build or docs**, update the relevant `README` / `README_DEV` so others can still reproduce builds.

## Code expectations (high level)

| Area | Notes |
|------|--------|
| **Kotlin / Compose** (`trierarch-app/`) | Follow Android/Kotlin idioms; keep UI logic readable. |
| **Rust** (`trierarch-native/`) | Run `cargo fmt` and `cargo clippy` where practical before submitting. |
| **C / NDK** (`trierarch-proot/`, `trierarch-wayland/`) | Match existing style; avoid unrelated reformatting in the same commit as logic fixes. |

## Documentation

- User-facing behaviour: update root [`README.md`](README.md) / [`README.zh.md`](README.zh.md) if needed.
- Build instructions: [`README_DEV.md`](README_DEV.md) and the **module** `README` under each directory.
- Plasma/rootfs tips: [`trierarch-optimize/`](trierarch-optimize/).

## License

By contributing, you agree that your contributions are licensed under the same terms as the project (see root [`LICENSE`](LICENSE)).

## Questions

If something is unclear, open an **issue** with the **question** label (or describe it as a question in the title)—that helps future contributors too.
