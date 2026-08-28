#!/usr/bin/env bash
# Refreshes the vendored third-party sources in lib/ and SDK/ from their pinned
# upstream versions.
#
# This is a maintainer tool, never a build step. The vendored copies stay
# committed so that a fresh clone builds offline with no setup, and so that
# checking out an old tag still reproduces the binary that shipped with it.
# Run this deliberately, review the diff, commit the result.
#
#   scripts/update-vendor.sh          refresh the tree in place
#   scripts/update-vendor.sh --check  report drift and change nothing
set -euo pipefail

# --- pinned versions -------------------------------------------------------
# Bumping one of these is the update. Everything below just fetches it.
IMGUI_REF="v1.92.8"
# stb publishes no tags or releases; this commit is stb_image v2.19.
STB_COMMIT="fa2a1d9b3be0733b222fc14a7428e81ce940c2a7"
DOCTEST_REF="v2.5.3"
# X-Plane SDK 4.3.0. Laminar serves this from an undocumented path that has
# changed between releases, so a 404 here means the URL moved, not that the
# version is gone.
SDK_VERSION="430"

# lib/ImgWindow is deliberately absent from this script. Upstream
# (xsquawkbox/xsb_public) has not been touched since July 2020 and targets a
# much older Dear ImGui; our copy is a substantial derivative carrying the
# ImGui 1.90+ adaptation, and SystemGL.h is not shipped upstream at all.
# Re-fetching it would silently break the build. Treat it as project source.

usage() {
    echo "usage: $0 [--check]" >&2
    exit 2
}

check_only=false
case "${1:-}" in
    --check) check_only=true ;;
    "") ;;
    *) usage ;;
esac
[[ $# -le 1 ]] || usage

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
staging="$(mktemp -d)"
trap 'rm -rf "$staging"' EXIT

fetch() { # url destination
    mkdir -p "$(dirname "$2")"
    curl -sSfL --retry 3 -o "$2" "$1"
}

stage_imgui() {
    local base="https://raw.githubusercontent.com/ocornut/imgui/$IMGUI_REF"
    local f
    for f in imgui.cpp imgui.h imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp \
             imgui_internal.h imconfig.h imstb_rectpack.h imstb_textedit.h \
             imstb_truetype.h LICENSE.txt; do
        fetch "$base/$f" "$staging/lib/imgui/$f"
    done
    for f in imgui_stdlib.cpp imgui_stdlib.h; do
        fetch "$base/misc/cpp/$f" "$staging/lib/imgui/misc/cpp/$f"
    done
}

stage_stb() {
    fetch "https://raw.githubusercontent.com/nothings/stb/$STB_COMMIT/stb_image.h" \
        "$staging/lib/stb/stb_image.h"
    # The pinned commit predates stb having a LICENSE file, so the licence text
    # comes from the current tree instead. Its terms have not changed.
    fetch "https://raw.githubusercontent.com/nothings/stb/master/LICENSE" \
        "$staging/lib/stb/LICENSE"
}

stage_doctest() {
    local base="https://raw.githubusercontent.com/doctest/doctest/$DOCTEST_REF"
    fetch "$base/doctest/doctest.h" "$staging/lib/doctest/doctest.h"
    fetch "$base/LICENSE.txt" "$staging/lib/doctest/LICENSE.txt"
}

stage_sdk() {
    local url="https://developer.x-plane.com/wp-content/plugins/code-sample-generation/sdk_zip_files/XPSDK${SDK_VERSION}.zip"
    fetch "$url" "$staging/sdk.zip"
    unzip -q "$staging/sdk.zip" -d "$staging/sdkroot"
    # The archive already contains a top-level SDK/ directory.
    mv "$staging/sdkroot/SDK" "$staging/SDK"
    rm -rf "$staging/sdk.zip" "$staging/sdkroot"
}

echo "fetching pinned sources..."
stage_imgui
stage_stb
stage_doctest
stage_sdk

targets=(lib/imgui lib/stb lib/doctest SDK)
drift=0

for target in "${targets[@]}"; do
    if diff -qr --no-dereference "$staging/$target" "$repo_root/$target" > /dev/null 2>&1; then
        printf '  %-14s up to date\n' "$target"
        continue
    fi
    drift=1
    if [[ "$check_only" == true ]]; then
        printf '  %-14s DRIFT\n' "$target"
        # --no-dereference keeps the macOS framework symlinks from reading as
        # directory loops. diff exits non-zero on difference, expected here;
        # without the guard set -e would abort before the remaining targets.
        diff -qr --no-dereference "$staging/$target" "$repo_root/$target" 2>&1 | sed 's/^/      /' || true
    else
        rm -rf "${repo_root:?}/$target"
        mkdir -p "$(dirname "$repo_root/$target")"
        cp -R "$staging/$target" "$repo_root/$target"
        printf '  %-14s updated\n' "$target"
    fi
done

echo
if [[ "$check_only" == true ]]; then
    if [[ "$drift" -eq 0 ]]; then
        echo "vendored sources match their pins."
    else
        echo "vendored sources differ from their pins (see above)." >&2
        exit 1
    fi
elif [[ "$drift" -eq 0 ]]; then
    echo "nothing changed; vendored sources already match their pins."
else
    cat <<'NOTE'
Updated. Before committing:
  - re-read lib/README.md and correct anything it now misstates
  - build all three platforms and run tests/run_tests.sh
  - ImGui and lib/ImgWindow are coupled; an ImGui bump often needs
    matching changes in our ImgWindow derivative
NOTE
fi
