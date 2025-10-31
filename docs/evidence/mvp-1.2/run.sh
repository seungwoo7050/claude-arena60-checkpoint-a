#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR="$ROOT/server/build"

if [ ! -d "$BUILD_DIR" ]; then
  mkdir -p "$BUILD_DIR"
  cmake -S "$ROOT/server" -B "$BUILD_DIR"
fi

cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure
