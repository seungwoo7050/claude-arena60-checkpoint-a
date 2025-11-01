#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR=${BUILD_DIR:-server/build}
cmake -S server -B "$BUILD_DIR" -DENABLE_COVERAGE=ON
cmake --build "$BUILD_DIR"
cd "$BUILD_DIR"
ctest --output-on-failure
ctest --output-on-failure -L performance
