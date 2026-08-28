# SafeFlightN1 faceplate art

Recreation of the Safe Flight N1 Computer (P/N C-12732-1) faceplate for the
plugin popup window, authored as SVG and rendered to PNG at 3x physical scale.

## Files

- `faceplate.svg` / `faceplate.png` — 1200x936. Charcoal plate with
  chamfered octagonal face, engraved legends, empty display recess (the
  plugin draws the digits), corner screws, Safe Flight oval logo. No knob.
- `knob.svg` / `knob.png` — 290x290, transparent background. Black mode knob:
  two flat facets split by a chord above center, with an ivory index scribed
  down the full diameter, pointing up. The knob circle fills the canvas edge
  to edge.
- `layout.json` — normalized hit/draw rects consumed by `window.cpp`
  (schema in `docs/CONTRACTS.md`).
- `fonts/Jost-VariableFont_wght.ttf` + `fonts/OFL.txt` — the legend typeface,
  vendored so the plate re-renders identically without a system font install.
  Build-time only; the plugin ships the PNGs, not the font.

## Re-rendering

From `assets/`:

```sh
CHROME='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
"$CHROME" --headless=new --disable-gpu --hide-scrollbars \
  --default-background-color=00000000 --window-size=1200,936 \
  --screenshot=faceplate.png "file://$PWD/faceplate.svg"
"$CHROME" --headless=new --disable-gpu --hide-scrollbars \
  --default-background-color=00000000 --window-size=290,290 \
  --screenshot=knob.png "file://$PWD/knob.svg"
```

`faceplate.svg` pulls Jost in through an `@font-face` rule with a path relative
to `assets/`, so render it from this directory. `rsvg-convert` resolves fonts
through fontconfig and ignores that rule; headless Chrome is the reference
renderer because it honours `@font-face` and finds the macOS-bundled
Baskerville the logo needs.

## Font choice

- **Legends: Jost, weight 430** — the real unit uses classic engraved
  instrument lettering (MS33558-style geometric sans, letterspaced caps).
  Jost is a Futura-like geometric sans under the SIL Open Font License, so
  unlike Futura it can be vendored into the repo and redistributed. It is a
  variable font, which is why the weight is an odd number: the lettering in
  `reference/photos/crop_face_big.jpg` measures a stem-width-to-cap-height
  ratio of 0.129, and 430 is the weight that reproduces it (a 5x render of
  the plate measures 0.129). Named weights bracket it uselessly — Jost
  Regular gives 0.118 and Medium 0.159. `PUSH TEMP SET` is the one legend
  the real plate sets lighter than the rest, so it drops to 400. Jost's
  digit `1` is a plain vertical bar with no flag and no foot serif, which is
  exactly how the real plate engraves the `1` of `N1`.
- **Logo: Baskerville Bold Italic** — stands in for Safe Flight's italic
  wordmark inside the heavy oval. The rule under the wordmark is straight,
  not an arc; it runs flush with the wordmark's ink at both ends, and its
  right end tapers to a tip with a barb hanging below — a half-arrowhead on
  the underside, sweeping back down and to the left. The logo is isolated in
  `<g id="safeflight-logo">` so it can be restyled or removed independently.

## Photo-measured dimensions

A record of where every number in the SVGs came from, kept so the art can be
rebuilt or corrected without re-deriving it. The `reference/` photographs it
cites are not redistributable and are not in the repository.

Measured from `reference/photos/c12732_1.jpg` (straight-on spare unit),
`reference/photos/jetaviva/IMG_2762_dom2nu.jpg` (straight-on in panel) and
`reference/photos/jetaviva/zoom_n1_big.jpg` (powered, best lighting). The
eBay photo is vertically foreshortened (camera above the face), so horizontal
ratios come from it and vertical ratios from the in-panel photos.

- Plate aspect: height = 0.78 x width (in-panel straight-on). At the nominal
  2.6 in physical width that is 2.6 x 2.03 in; 3x scale gives 1200 x 936 px.
- Plate: rectangle with 45-degree-chamfered octagonal face; chamfer spans
  0.24 W horizontally, 0.26 H vertically. Corner triangles carry the four
  mounting screws (empty countersinks on the spare, black screws installed).
  The chamfer facets are bare — no holes or fasteners on them.
- Display recess (inner dark opening): x 0.300, y 0.221, w 0.365, h 0.246 —
  a ~1.9:1 letterbox centered at 0.4825 W, slightly left of plate center, as
  on the real unit. Opening width = 0.362 of plate width in c12732_1.jpg;
  the aspect is averaged from the powered in-panel photos (1.8-2.0). The
  opening is sunk into the plate: a 24 px wall around it runs from deep
  shadow at the top edge (that wall faces away from the overhead light) to a
  lit face at the bottom edge, with a crisp break where the plate surface
  ends.
- Lettering sizes (fractions of plate width, from c12732_1.jpg): title 0.42
  wide with cap height ~0.10 of its width, TO/GA 0.14, CLB/CRU 0.08,
  PUSH TEMP SET 0.31. The glyphs are drawn as-is: with the weight dialled to
  the measured stroke ratio there is nothing left for an overstroke to
  correct.
- Everything except the plate outline centers on x ~ 0.483 W: title, display
  window, mode arc, knob shaft.
- Mode arc: labels CLB (0.257, 0.61), TO/GA (0.485, 0.57), CRU (0.717, 0.61).
  Each leader runs in horizontally at its label's optical centerline and then
  turns onto its own detent radius, so the knob's index lies along the leader
  when that mode is selected — elbow at the point where the detent ray crosses
  y = 0.611 H, then on under the knob skirt, as on the real unit.
- Knob: shaft center (0.485, 0.771), radius 0.1208 W (0.63 in physical,
  matching the in-panel knob at ~0.25 of plate width). The face is two flat
  facets meeting at a chord 0.22 r above center: the upper one is tilted and
  so reads brighter, the lower one is parallel to the view plane. Neither is
  curved, so each takes a single flat tone and the chord is only where those
  tones meet — it is not a drawn line. The index is scribed down the full
  diameter across both facets and shifts tone with the facet it crosses. The
  cylindrical rim is knurled on the real knob, but that edge is invisible
  looking straight down the axis, so it is not drawn. Facets and index rotate
  together, which keeps the sprite self-consistent through the detent throw.
  The knob PNG's circle fills its canvas, so draw it at 2r x 2r scaled to the
  faceplate and rotate about its center. The plate carries the knob's contact
  shadow (`#knob-seat`), sitting 5 px low because the light is overhead — a
  black knob on a black plate has almost no edge contrast without it.
- Mode detent angles: -40 / 0 / +40 degrees (CLB / TO-GA / CRU). The two
  reference units bracket the throw at 32 and 46 degrees — both photos are
  oblique, in opposite directions — and 40 sits between them while leaving
  the leaders the long-horizontal, short-hook proportions the plate shows.
  The same angles drive `layout.json`, so the art and the pointer cannot
  drift apart.
- Safe Flight oval: center (0.215, 0.788), 0.25 W x 0.12 H, heavy outline.
  Its right edge sits 0.024 W clear of the knob skirt; both reference units
  show that gap (logo right edge 0.332 W straight-on, knob skirt starting at
  0.364 W), so the installed knob never crosses the oval. Inside it the
  wordmark spans 0.76 of the oval width — measured on the spare unit by
  background-subtracting the photo's lighting falloff, which a plain
  brightness threshold reads as a rule stopping two thirds of the way across.
  The barb at the rule's right end is drawn at about twice its photographed
  reach, which is a hair over 2 rule-thicknesses: at the size the plugin
  draws the plate, the measured barb disappears.
- PUSH TEMP SET baseline at 0.965 H; on the real unit the knob skirt grazes
  this line (the eBay photo shows TEMP partially hidden straight-on).
- Colors: plate charcoal #2b2b2d +/- highlight/shadow; legends aged white
  #e8e7e1; recess near-black #070708.
