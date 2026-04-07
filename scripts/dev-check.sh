#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "== Rust (trierarch-native): fmt, clippy, check =="
(cd "$ROOT/trierarch-native" && cargo fmt --check)
(cd "$ROOT/trierarch-native" && cargo clippy -- -D warnings)
(cd "$ROOT/trierarch-native" && cargo check)

echo ""
echo "== Android (trierarch-app): Kotlin compile, lint =="
(cd "$ROOT/trierarch-app" && ./gradlew :app:compileDebugKotlin :app:lintDebug --stacktrace)

echo ""
echo "OK"

