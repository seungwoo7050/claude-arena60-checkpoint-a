#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(git rev-parse --show-toplevel)
BUILD_DIR="$ROOT_DIR/server/build-evidence"

cmake -S "$ROOT_DIR/server" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD_DIR" -- -j"$(nproc)"

pushd "$BUILD_DIR" >/dev/null
ctest --output-on-failure
popd >/dev/null
