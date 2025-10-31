#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="$(dirname "$0")/../../server/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake ..
cmake --build .
ctest --output-on-failure
