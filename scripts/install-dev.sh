#!/usr/bin/env bash
# Symlinks dist/SafeFlightN1 into the X-Plane 12 plugins folder (idempotent).
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
src="$repo_root/dist/SafeFlightN1"
# No default: X-Plane lives wherever the user put it, which differs on every
# platform and every machine, so guessing only produces a confusing failure.
if [[ -z "${XPLANE_DIR:-}" ]]; then
    echo "error: set XPLANE_DIR to your X-Plane 12 installation" >&2
    echo "       e.g. XPLANE_DIR=\"/path/to/X-Plane 12\" $0" >&2
    exit 1
fi

dst="$XPLANE_DIR/Resources/plugins/SafeFlightN1"

if [[ ! -d "$XPLANE_DIR/Resources/plugins" ]]; then
    echo "error: $XPLANE_DIR does not look like an X-Plane installation" >&2
    echo "       (no Resources/plugins folder inside it)" >&2
    exit 1
fi

if [[ ! -d "$src" ]]; then
    echo "error: $src does not exist; run scripts/build.sh first" >&2
    exit 1
fi

if [[ -e "$dst" && ! -L "$dst" ]]; then
    echo "error: $dst exists and is not a symlink; remove it manually" >&2
    exit 1
fi

ln -sfn "$src" "$dst"
echo "installed: $dst -> $src"
