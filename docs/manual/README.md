# Producing the manual

`index.html` is the source of truth. The PDF is generated from it and is **not**
committed — the release workflow renders it into the download, and you can
render it locally:

```sh
scripts/render_manual.sh              # -> docs/manual/SafeFlightN1-Manual.pdf
```

That needs Google Chrome or Chromium, which the script finds on macOS and Linux;
set `CHROME` to override. Page size, margins and page breaks all come from
`manual.css` — headless Chrome always sets `preferCSSPageSize` — and Chrome
contributes only the footer page numbers and the PDF bookmarks that stand in for
a table of contents.

`jost-embedded.css` carries the Jost faceplate typeface inline as a data URI,
because Chrome refuses `@font-face` fetches over `file://`. Regenerate it with
`python3 scripts/embed_font.py` if the font in `assets/fonts/` changes.

## Screenshots

The manual's figures live in `docs/images/`, captured in X-Plane 12 with the
TorqueSim CitationJet 525 by `scripts/capture_manual_shots.sh`, which drives the
device through each display state via its `sfn1/test_override_*` datarefs.

| Image | State shown |
| --- | --- |
| `in-cockpit.png` | The popup floating over the CJ525 cockpit |
| `display-off.png` | Unpowered: dark display |
| `display-self-test.png` | `888` power-up self-test |
| `display-takeoff-n1.png` | TO/GA on the ground: takeoff N1 target |
| `display-dashes.png` | CLB selected on the ground: invalid mode |
| `display-anti-ice.png` | All bleed anti-ice on: anti-ice schedule |
| `display-climb.png` | Airborne CLB: max continuous climb target |
| `display-cruise.png` | Airborne CRU: max cruise target |
| `display-go-around.png` | Airborne TO/GA: go-around target |
| `display-fail.png` | Air data failed: blank but still energised |
| `display-rat.png` | Knob held: ram air temperature |
| `display-temp-set.png` | Knob held and rotated: selected temperature |

Re-capture with the popup at its default size and position:

```sh
SFN1_WINDOW_RECT=993,495,576,450 bash scripts/capture_manual_shots.sh
```

The rect is in logical points and depends on X-Plane's framebuffer size: at
5120x2880 a boxel is half the screen size it is at 2560x1440, so the popup lands
at half these dimensions and the rect has to be re-derived. To find it, capture
the whole screen and locate the amber readout — it sits at `display_window` in
`assets/layout.json`, which fixes the rest of the plate.
