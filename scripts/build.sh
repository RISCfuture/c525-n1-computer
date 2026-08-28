#!/usr/bin/env bash
# Builds the universal SafeFlightN1.xpl into dist/SafeFlightN1/mac_x64/.
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$repo_root" -B "$repo_root/build"
cmake --build "$repo_root/build" --parallel
