#!/usr/bin/env python3
"""Writes the SkunkCrafts Updater manifest for a packaged plugin folder.

The updater fetches ``skunkcrafts_updater.cfg`` from the plugin folder on the
user's disk, compares its ``version`` against the copy at the ``module`` URL,
and then downloads only the files whose CRC32 or size differs from the
whitelist and sizes list published alongside it.
"""

from __future__ import annotations

import argparse
import zlib
from pathlib import Path

# Files the updater manages itself; they never appear in their own manifest.
MANIFEST_PREFIX = "skunkcrafts_updater"


def crc32(path: Path) -> int:
    """CRC32 of a file's bytes, as the unsigned decimal the updater expects."""
    checksum = 0
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            checksum = zlib.crc32(chunk, checksum)
    return checksum


def payload_files(root: Path) -> list[Path]:
    """Every shipped file, sorted, excluding the updater's own manifest."""
    return sorted(
        path for path in root.rglob("*") if path.is_file() and MANIFEST_PREFIX not in path.name
    )


def write_manifest(root: Path, version: str, module_url: str, name: str) -> int:
    config = {
        "zone": "custom",
        "liveries": "false",
        "module": module_url,
        "version": version,
        "disabled": "false",
        "name": name,
        "locked": "false",
    }
    (root / f"{MANIFEST_PREFIX}.cfg").write_text(
        "".join(f"{key}|{value}\n" for key, value in config.items())
    )

    files = payload_files(root)
    checksums = []
    sizes = []
    for path in files:
        relative = path.relative_to(root).as_posix()
        checksums.append(f"{relative}|{crc32(path)}\n")
        sizes.append(f"{relative}|{path.stat().st_size}\n")

    (root / f"{MANIFEST_PREFIX}_whitelist.txt").write_text("".join(checksums))
    (root / f"{MANIFEST_PREFIX}_sizeslist.txt").write_text("".join(sizes))
    (root / f"{MANIFEST_PREFIX}_blacklist.txt").write_text("")
    return len(files)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="the packaged plugin folder")
    parser.add_argument("--version", required=True, help="version the updater compares")
    parser.add_argument("--module-url", required=True, help="where the files are served")
    parser.add_argument("--name", default="Safe Flight N1 Computer")
    args = parser.parse_args()

    if not args.root.is_dir():
        parser.error(f"not a directory: {args.root}")

    # The updater joins the module URL and a relative path without inserting a
    # separator, so a missing trailing slash silently breaks every download.
    module_url = args.module_url if args.module_url.endswith("/") else args.module_url + "/"

    count = write_manifest(args.root, args.version, module_url, args.name)
    print(f"wrote SkunkCrafts manifest for {count} files in {args.root}")


if __name__ == "__main__":
    main()
