#!/bin/bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_root/tests/build"
mkdir -p "$build_dir"

"${CXX:-clang++}" -std=c++20 -Wall -Wextra -Werror -g \
    -I "$repo_root/lib/doctest" \
    "$repo_root/src/n1_tables.cpp" \
    "$repo_root/src/n1_computer.cpp" \
    "$repo_root/tests/test_main.cpp" \
    -o "$build_dir/n1_tests"

# Passed through the environment so every argument belongs to doctest:
#   tests/run_tests.sh --list-test-cases
#   tests/run_tests.sh -tc="*anti-ice*"
export SFN1_FIXTURES_DIR="$repo_root/tests/fixtures"
export SFN1_DATA_DIR="$repo_root/data"

"$build_dir/n1_tests" "$@"
