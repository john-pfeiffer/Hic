#!/usr/bin/env bash
# Configure, build, run the tests, and render the kit for listening.
# Usage: scripts/verify.sh [render-dir]
set -euo pipefail
cd "$(dirname "$0")/.."
OUT="${1:-build/render}"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build
ctest --test-dir build --output-on-failure
if [ -x build/tests/render_kit ]; then
  mkdir -p "$OUT"
  build/tests/render_kit "$OUT"
fi
