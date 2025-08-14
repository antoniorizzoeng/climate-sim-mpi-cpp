#!/usr/bin/env bash
set -euo pipefail
CF=${CF:-clang-format-15}
if ! command -v $CF >/dev/null 2>&1; then
  CF=clang-format
fi
git ls-files \
  | grep -E '\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$' \
  | grep -vE '^(build/|_deps/|third_party/|bench/_deps/)' \
  | xargs -r $CF -i -style=file
