# Commenting guidelines (Trierarch)

Scope: **only Trierarch-maintained code**. Do **not** “normalize” comments in vendored/upstream trees.

Included:
- `trierarch-app/app/src/main/java/app/trierarch/**`
- `trierarch-native/src/**`
- `trierarch-wayland/src/**`

Excluded (examples):
- `trierarch-proot/**`
- `trierarch-wayland/libffi/**`
- `trierarch-wayland/libs/**`
- generated protocol headers under `trierarch-wayland/protocol/**`

## What to comment (priority order)

- **Boundary/ABI (MUST)**: JNI exports, public bridges, cross-language contracts.
  - Required: **call order**, **threading**, **lifecycle**, **error semantics**.
- **Non-obvious constraints (MUST)**: “must happen before”, invariants, ownership/locking rules.
- **Workarounds & trade-offs (SHOULD)**: why we swallow errors, why a cap exists, why a heuristic is used.
- **State machines (SHOULD)**: list states + transitions in a short block at the top of the file.

Avoid:
- **Narration (SHOULD NOT)**: comments that restate the code line-by-line.
- **Stale docs (SHOULD NOT)**: copying README into code. Link instead.
- **External-project references (SHOULD NOT)**: avoid naming other projects as behavioral anchors.
  Describe Trierarch behavior directly (paths, mounts, invariants).

## Kotlin (Compose / Android)

- Public API: use **KDoc** (`/** ... */`) describing the contract.
- Internal logic: prefer short `// why` comments at decision points.
- State machines: add a brief **“flow” block** at the top of the file (5–15 lines).

Example (contract, not narration):

```kotlin
/**
 * Preconditions: proot spawned; Wayland server started.
 * Idempotent: will not re-run Display script after a desktop client connects.
 */
fun runDisplayStartupScriptIfNeeded() { /* ... */ }
```

## Rust (native / JNI)

- Module-level intent: `//!`
- Exported functions / public APIs: `///` with:
  - **threading** (which thread calls it, what it spawns)
  - **resource ownership** (who drops/kills what)
  - **failure behavior** (what state is left behind)

Example:

```rust
/// Spawns proot and starts a PTY reader thread.
///
/// Threading: called from JNI; spawns one Rust thread for PTY output.
/// Failure: returns an error and leaves previous child (if any) dropped.
pub fn spawn_proot() -> Result<()> { /* ... */ }
```

## C (Wayland compositor / JNI bridge)

- File header: 1–5 lines: “module responsibility + key constraints”.
- For exported JNI functions and cross-thread mechanisms:
  - Document **thread ownership**, **lock/queue rules**, and **budget/caps** (for performance safety).

Example:

```c
/* libwayland-server is not thread-safe; enqueue from JNI threads, drain on Wayland thread. */
```

## “Definition of done” for a new feature

- Boundary/API changes have contract comments.
- Any new background thread / queue has a short “why + limits” comment.
- Any new heuristic/constant has a one-line reason (what it protects against).

