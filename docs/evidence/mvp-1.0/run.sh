#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/server/build"
COVERAGE_DIR="$ROOT_DIR/server/build-coverage"

cmake -S "$ROOT_DIR/server" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -- -j"$(nproc)"
ctest --test-dir "$BUILD_DIR" --output-on-failure

cmake -S "$ROOT_DIR/server" -B "$COVERAGE_DIR" -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build "$COVERAGE_DIR" -- -j"$(nproc)"
ctest --test-dir "$COVERAGE_DIR" --output-on-failure
python3 -m gcovr --object-directory "$COVERAGE_DIR" --root "$ROOT_DIR" \
  --filter 'server/src' --exclude-directories 'server/build-coverage/CMakeFiles' \
  --exclude-directories 'server/build-coverage/Testing' --xml "$ROOT_DIR/coverage.xml" \
  --html-details "$ROOT_DIR/coverage.html" --fail-under-line 70
mkdir -p "$ROOT_DIR/docs/evidence/mvp-1.0/coverage"
cp "$ROOT_DIR/coverage.html" "$ROOT_DIR/docs/evidence/mvp-1.0/coverage/index.html"

ARENA60_PORT=0 ARENA60_METRICS_PORT=9100 "$BUILD_DIR"/src/arena60_server \
  > "$ROOT_DIR/metrics.log" 2>&1 &
SERVER_PID=$!
sleep 1
curl -s "http://127.0.0.1:9100/metrics" > "$ROOT_DIR/docs/evidence/mvp-1.0/metrics.txt"
kill "$SERVER_PID"
wait "$SERVER_PID" 2>/dev/null || true
