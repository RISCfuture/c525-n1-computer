#!/usr/bin/env python3
"""Regenerate docs/manual/jost-embedded.css from the vendored Jost font.

Chrome refuses @font-face fetches over file://, so the manual carries the font
inline as a data URI rather than linking assets/fonts/.
"""

import base64
import pathlib

REPO = pathlib.Path(__file__).resolve().parent.parent
FONT = REPO / "assets/fonts/Jost-VariableFont_wght.ttf"
OUT = REPO / "docs/manual/jost-embedded.css"

HEADER = """\
/* Jost, vendored from assets/fonts/Jost-VariableFont_wght.ttf and embedded as a
   data URI. Chrome refuses @font-face fetches over file:// no matter where the
   file sits or which flags are passed, so a linked .ttf would silently fall back
   to Helvetica in both the PDF and a double-clicked index.html. Regenerate with
   scripts/embed_font.py after replacing the font.
   Jost is licensed under the SIL Open Font License; see assets/fonts/OFL.txt. */
"""


def main() -> None:
    b64 = base64.b64encode(FONT.read_bytes()).decode()
    OUT.write_text(
        HEADER
        + "@font-face {\n"
        + '    font-family: "Jost";\n'
        + f'    src: url("data:font/ttf;base64,{b64}") format("truetype");\n'
        + "    font-weight: 100 900;\n"
        + "    font-style: normal;\n"
        + "    font-display: swap;\n"
        + "}\n"
    )
    print(f"wrote: {OUT.relative_to(REPO)} ({OUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
