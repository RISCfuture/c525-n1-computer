#!/usr/bin/env bash
# Assembles the shippable SafeFlightN1 folder from built artifacts.
#
# Expects the layout actions/download-artifact produces:
#   <artifacts>/xpl-mac/SafeFlightN1.xpl
#   <artifacts>/xpl-win/SafeFlightN1.xpl
#   <artifacts>/xpl-lin/SafeFlightN1.xpl
#   <artifacts>/manual/SafeFlightN1-Manual.pdf
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <artifacts-dir> <output-dir>" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
artifacts="$1"
plugin="$2/SafeFlightN1"

require() {
    [[ -f "$1" ]] || { echo "error: missing $1" >&2; exit 1; }
}

for platform in mac win lin; do
    require "$artifacts/xpl-$platform/SafeFlightN1.xpl"
done
require "$artifacts/manual/SafeFlightN1-Manual.pdf"

rm -rf "$plugin"
mkdir -p "$plugin"/{mac_x64,win_x64,lin_x64,data,assets}

for platform in mac win lin; do
    install -m 0755 "$artifacts/xpl-$platform/SafeFlightN1.xpl" \
        "$plugin/${platform}_x64/SafeFlightN1.xpl"
done

# The tables and the art the plugin reads at runtime, and the terms they carry.
install -m 0644 "$repo_root"/data/*.csv "$plugin/data/"
install -m 0644 "$repo_root/data/PROVENANCE.md" "$repo_root/data/LICENSE.md" "$plugin/data/"
install -m 0644 \
    "$repo_root/assets/faceplate.png" \
    "$repo_root/assets/knob.png" \
    "$repo_root/assets/layout.json" \
    "$plugin/assets/"

install -m 0644 "$artifacts/manual/SafeFlightN1-Manual.pdf" "$plugin/"
install -m 0644 "$repo_root/LICENSE" "$plugin/"
# The end-user README ships as README.md; the repository's own is for developers.
install -m 0644 "$repo_root/docs/dist-README.md" "$plugin/README.md"

echo "packaged: $plugin"
find "$plugin" -type f | sed "s|^$plugin/|  |" | sort
