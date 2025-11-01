#!/usr/bin/env bash
set -euo pipefail
mapfile -t FILES < <(git ls-files '*.c' '*.cc' '*.cpp' '*.h' '*.hpp')
if (( ${#FILES[@]} )); then
  clang-format -style=file -i "${FILES[@]}"
fi
