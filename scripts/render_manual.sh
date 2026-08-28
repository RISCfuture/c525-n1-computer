#!/usr/bin/env bash
# Renders docs/manual/index.html to a print-ready PDF with headless Chrome.
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
src="$repo_root/docs/manual/index.html"
out="${1:-$repo_root/docs/manual/SafeFlightN1-Manual.pdf}"

find_chrome() {
    if [[ -n "${CHROME:-}" ]]; then
        printf '%s' "$CHROME"
        return
    fi
    local candidate
    for candidate in \
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" \
        google-chrome google-chrome-stable chromium chromium-browser; do
        if command -v "$candidate" > /dev/null 2>&1; then
            command -v "$candidate"
            return
        fi
    done
}

chrome="$(find_chrome)"

if [[ -z "$chrome" || ! -x "$chrome" ]]; then
    echo "error: no Chrome found; set CHROME to its path" >&2
    exit 1
fi

if [[ ! -f "$src" ]]; then
    echo "error: $src does not exist" >&2
    exit 1
fi

# Page size and margins come from the stylesheet's @page rule: headless
# print-to-pdf always sets preferCSSPageSize and passes no paper overrides.
# --no-pdf-header-footer drops Chrome's own URL/date furniture; the older
# --print-to-pdf-no-header no longer exists and is silently ignored.
# --generate-pdf-document-outline turns the heading tree into PDF bookmarks,
# which is what stands in for a table of contents with page numbers.
args=(
    --headless
    --disable-gpu
    --no-pdf-header-footer
    --generate-pdf-document-outline
    --virtual-time-budget=10000
)

# CI images from Ubuntu 23.10 on disable unprivileged user namespaces, so
# Chrome aborts with "No usable sandbox!" unless it is passed --no-sandbox.
# That belongs to the environment, not to this script: rendering a local file
# with the sandbox off is fine on a disposable runner and not something to do
# silently on a workstation.
if [[ -n "${CHROME_EXTRA_ARGS:-}" ]]; then
    read -ra extra_args <<< "$CHROME_EXTRA_ARGS"
    args+=("${extra_args[@]}")
fi

args+=(--print-to-pdf="$out" "file://$src")

"$chrome" "${args[@]}"

echo "wrote: $out"
