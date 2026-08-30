# Data provenance — N1 tables

All values were transcribed visually from
`reference/TorqueSim Cessna Citation 525 Performance Manual.pdf` (37 PDF pages),
rendered at 300 dpi for every cell that required adjudication. Black print = normal
schedule, red print = ANTI-ICE ON (`_ai` files). A red dash or an absent red row =
no charted data = empty CSV cell.

That manual is community work by **hornetaircraft**, assisted by
**Aviationsocal**, **Kaboom**, **Jetpipeoverheat** and **Cptlee**, published
free on the [X-Pilot
forums](https://forums.x-pilot.com/files/file/1613-torquesim-cessna-citation-525-performance-manual/)
in December 2024. It is not a TorqueSim, X-Aviation or Textron document, and it
does not ship with the aircraft; see [`LICENSE.md`](LICENSE.md).

## Source pages

| File | Source |
| --- | --- |
| `n1_takeoff.csv` | TAKEOFF pages, "T.O. Power" black row: PDF pp. 6/7 (SL), 8/9 (1000 ft), 10/11 (2000), 12/13 (3000), 14/15 (4000), 16/17 (5000), 18/19 (6000), 20/21 (7000). Each altitude printed twice (FLAPS 0 / FLAPS 15). |
| `n1_takeoff_ai.csv` | Same pages, "T.O. Power" red row. |
| `n1_goaround.csv` | LANDING pages, "GA PWR" black row: PDF pp. 27 (SL), 28 (1000 ft), 29 (2000), 30 (3000), 31 (4000), 32 (5000), 33 (6000), 34 (7000). |
| `n1_goaround_ai.csv` | Same pages, "GA PWR" red row. |
| `n1_climb.csv` / `n1_climb_ai.csv` | PDF p. 22, "RECOMMENDED MAX CLIMB THRUST SETTING N1% RPM" (black / red). |
| `n1_cruise.csv` / `n1_cruise_ai.csv` | PDF p. 23, "RECOMMENDED MAX CRUISE THRUST SETTING N1% RPM" (black / red). |

PDF pp. 25-26 are landing field-length tables (no N1 data); p. 24 is climb speeds.

## Takeoff flaps-page cross-check

Every altitude's "T.O. Power" rows were read on both the FLAPS 0 and FLAPS 15 page.
Results:

- **SL, 1000, 2000, 4000 ft** — both copies identical.
- **3000 ft, -20 °C (black and red)**: FLAPS 0 (p. 12) prints **94.2**, FLAPS 15
  (p. 13) prints **94.3**. The PDF is internally inconsistent. Adopted **94.2**:
  the landing page's GA PWR row for 3000 ft (p. 30), an independent third copy,
  prints 94.2, and the S.E.-climb-minus-T.O. offset (+0.2 at neighbouring
  altitudes) also fits 94.2.
- **5000 ft, -10 °C black**: FLAPS 0 (p. 16) renders as a garbled "991.1"; FLAPS 15
  (p. 17) prints **99.1**. Adopted 99.1 (also corroborated by the 5000-ft GA row).
- **6000 ft, 0 °C red**: FLAPS 0 (p. 18) prints **95.7** — a duplication of the
  adjacent -10 °C cell; FLAPS 15 (p. 19) prints **94.3**, matching the 94.3
  anti-ice cap at every other altitude from 1000-7000 ft. Adopted 94.3.
- **7000 ft (p. 20, FLAPS 0)** carries a literal "**TO DO STILL**" annotation and is
  unfinished: its red T.O. row (92.4, 94.2, 95.7, 94.3, 92.7) is a copy of the
  3000-ft red row, and its black -10 °C cell prints 102.2 where FLAPS 15 (p. 21)
  prints 100.2. Adopted the FLAPS 15 red row (**98.6, 97.4, 95.7, 94.3, 92.7**,
  which continues the 6000-ft clip pattern 97.4/95.7/94.3) and black -10 °C
  **102.2** (monotone altitude trend 99.1 → 100.7 → 102.2; 100.2 on p. 21 would
  decrease with altitude and is a digit transposition; the 7000-ft GA row on p. 34
  also prints 102.2).

## Go-around decision

The manual **does** chart go-around N1: every landing page (pp. 27-34) has a
"GA PWR" black/red row with temperature columns **-30**, -20, -10, 0, 10, 20, 30,
40 °C. `n1_goaround{,_ai}.csv` were transcribed from those rows — not derived from
the takeoff table. This -30 °C column is where Matchstick's extra row came from.

GA differs from takeoff only at: the added -30 °C column; red 10 °C = **92.8** at
SL/1000/3000/4000/5000 ft (takeoff prints 92.7; 2000/6000/7000 ft print 92.7);
red -30 °C at 3000 ft = **92.3** and 5000 ft = **95.2** (0.1 below their black
values, as printed); black 20 °C at 6000 ft = **97.8** (takeoff prints 97.9) and
black 0 °C at 7000 ft = **100.8** (takeoff prints 100.9) — both kept as printed
since neither is provably wrong.

**Corrected cell block — 2000 ft (p. 29):** the printed GA row (black
90.8, 91.7, 93.5, 95.3, 97.0, ... / red 90.8, 91.7, 93.5, 94.3, 92.7) repeats the
1000-ft page's values verbatim for every column except -30 °C — while at all seven
other altitudes the GA row equals that altitude's takeoff row. This is a
copy-paste defect in the manual (GA thrust cannot be 1.2-1.3 %N1 below takeoff
thrust at the same conditions; the AFM figure is one "Takeoff/Go Around Thrust
Setting" chart). Adopted the 2000-ft takeoff values for -20…10 °C: black
92.9, 94.7, 96.5, 98.3; red 92.9, 94.7 (0 and 10 °C printed cells 94.3/92.7
already equal the takeoff values). The printed -30 °C value 90.8 was kept.

## Corrected PDF typos (climb / cruise)

Three cells are deliberately not what the PDF prints; each was verified at high
zoom before correcting, and each is an obvious digit transposition or row-internal
duplication:

| Table | Cell | PDF prints | Written | Why |
| --- | --- | --- | --- | --- |
| Climb p. 22 | -25 °C / SL black | 98.9 | **89.9** | Neighbours -20 → 90.7, -30 → 89.0; SL exceeds this row's own 5000-ft value (96.1) in a regime where SL is always lowest. Digit transposition of 89.9. |
| Climb p. 22 | -25 °C / SL red | 98.7 | **89.7** | Same cell block; red equals black at SL in every surrounding row. |
| Cruise p. 23 | -25 °C / SL red | 98.7 | **89.7** | Black prints 89.7; red = black at SL in every surrounding row. |
| Cruise p. 23 | -30 °C / 10,000 ft black | 95.2 | **100.8** | Duplicates the row's 5000-ft cell; the red row prints 100.8 there (red never exceeds black), and both neighbours (-25 → 100.3, -35 → 101.5) bracket 100.8. |

All other oddities were kept as printed, e.g. climb 35 °C = 91.2 flat while cruise
35 °C = 91.3 (91.2 above 35,000 ft); cruise 10 °C = 95.8 through 10,000 ft then
95.9 (climb switches at 5000 ft); climb -45 °C red SL/5000 (85.9/92.2) below
black (86.1/92.9).

## Matchstick diff (`reference/matchstick/CJ5252_Data`)

The comparison set is the data shipped with **Matchstick's** [CJ525 N1
Calculator](https://forums.x-pilot.com/files/file/1635-cj525-n1-calculator/),
the FlyWithLua script that first brought these schedules into X-Plane.

Matchstick's layout differs: TO/GA tables are altitude-rows × temperature-columns
with anti-ice as `-25A`-style suffix columns; climb/cruise are temperature-rows ×
altitude-columns with `0A`-style suffix columns; `-` marks empty cells. After
mapping their layout onto ours (transpose + suffix-column split), every cell was
compared numerically. 13 cells disagree; everything else matches exactly.

Where the PDF supports this repo's value (Matchstick transcription errors):

| Table | Cell | Mine (PDF) | Matchstick | Note |
| --- | --- | --- | --- | --- |
| TO ai | -25 °C / 7000 ft | 98.6 | 96.8 | p. 21 prints 98.6; theirs looks copied from the 6000-ft row. |
| TO ai | -20 °C / 7000 ft | 97.4 | 92.4 | p. 21 prints 97.4; theirs comes from the unfinished "TO DO STILL" FLAPS 0 page (which prints 92.4 at -25). |
| GA | 0 °C / 7000 ft | 100.8 | 100.9 | p. 34 prints 100.8; they substituted the takeoff value. |
| GA | 20 °C / 6000 ft | 97.8 | 97.9 | p. 33 prints 97.8; they substituted the takeoff value. |
| Climb ai | 10 °C / SL | 93.3 | 95.3 | p. 22 red row prints 93.3, 93.3, 93.3 for SL/5000/10,000. |
| Climb ai | 10 °C / 5000 ft | 93.3 | 95.3 | Same row. |
| Cruise | -35 °C / 5000 ft | 94.2 | 93.2 | p. 23 prints 94.2 (their own -35 anti-ice column at 5000 ft is 94.2 — internally inconsistent). |
| Cruise | -30 °C / SL | 88.9 | 89.9 | p. 23 prints 88.9 (their own anti-ice cell is 88.9). |
| Cruise | 10 °C / 5000 ft | 95.8 | 95.9 | p. 23 prints 95.8 through 10,000 ft. |
| Cruise | 10 °C / 10,000 ft | 95.8 | 95.9 | Same row. |

Where the difference is a deliberate correction on our side:

| Table | Cell | Mine | Matchstick | Note |
| --- | --- | --- | --- | --- |
| Climb ai | -25 °C / SL | 89.7 | 98.7 | PDF prints 98.7 (transposition, see above). Matchstick copied the typo — yet corrected the black twin cell to 89.9, matching our reading of the same defect. |
| GA ai | -20 °C / 2000 ft | 92.9 | 91.7 | 2000-ft copy-paste row: Matchstick corrected the black row to takeoff values but left the red row as printed. |
| GA ai | -10 °C / 2000 ft | 94.7 | 93.5 | Same. |

Matchstick's `N1GATable.csv` -30 °C column (88.8, 89.9, 90.8, 92.4/92.3, 93.7,
95.3/95.2, 96.8, 98.6) matches the landing pages' GA PWR rows cell-for-cell —
their "extra" data came from PDF pp. 27-34, the same source used here.

## Verification

An independent adversarial pass (stream B2) re-read every source page directly
from the PDF and checked every CSV cell against it, treating the PDF as the only
authority.

**Method.** The PDF is digitally generated (real text objects, not scans), so
every N1 cell was extracted programmatically with PyMuPDF 1.28.2 as
(text, x, y, color) spans and mapped to its temperature/altitude column by
position; black (`000000`) and red (`ff0000`) rows were classified per span, so
the normal-vs-anti-ice split was verified for every cell, not sampled. A second
independent read used `pdftotext -layout` on all 26 source pages. Every cell
that decides an adjudication was additionally rendered at 5x zoom and inspected
visually (pp. 12, 13, 16, 18, 20, 21 T.O. Power rows; pp. 29, 30, 32, 34 GA PWR
rows; p. 22 -25/-35 °C rows; p. 23 -25/-30 °C rows).

**Coverage.** 1,092 CSV cells (values and empties) across all eight files were
compared against the PDF; for takeoff, both the FLAPS 0 and FLAPS 15 copy of
every row were parsed and cross-compared, so each takeoff cell was checked
against both printed instances. Pages 1-5, 24-26, and 35-37 were scanned to
confirm no other N1 schedule exists (p. 3's "Takeoff N1 95.9%" block is the
simplified-criteria quick reference, not a schedule).

**Errors found: 0.** Every CSV cell matches the PDF, and at every documented
adjudication the PDF prints exactly the values the tables above claim
(including p. 16's literal "991.1" garble, p. 20's "TO DO STILL" row copied
from 3000 ft, the p. 12/13 94.2-vs-94.3 disagreement, p. 29's copy-of-p. 28 GA
row, and all four climb/cruise typos). The 13-cell Matchstick diff was
re-confirmed on our side: in every case the PDF prints this repo's value.

**Structural checks (automated).** Axes monotonic and exactly as contracted;
row/column counts correct in all files; all cells one-decimal; no trailing
commas; Unix newlines; `_ai` empties form a contiguous warm-side suffix in
every column; base tables have no empty cells. Two heuristic findings, both
verified against the page and kept as printed:

- Six anti-ice cells fall below a nominal 85-105 sanity band (minimum 81.8 at
  climb 10 °C / 25,000 ft); all are genuinely printed in the PDF. Consumers
  must not assume 85 as a floor.
- One cell has anti-ice N1 above normal N1: climb -35 °C / 15,000 ft prints
  black 100.2, red 100.3. Confirmed at 5x zoom on p. 22 — the manual really
  prints this pair; kept as printed.

**Remaining doubts.** Only the two judgment calls already documented above:
the 2000-ft GA block (pp. 29 values -20…10 °C replaced with the takeoff row —
an inference from the other seven altitudes where GA equals takeoff, supported
by p. 29's printed red 10 °C cell 92.7 equalling the takeoff value rather than
p. 28's 92.8) and the 7000-ft takeoff adjudications sourced from the "TO DO
STILL" page pair. Both are inherently unresolvable from the PDF alone and are
flagged rather than silently trusted.

## In-sim oracle comparison (2026-08-28)

TorqueSim's built-in takeoff calculator exposes `afm/cj/debug/perf/out_n1`, which the
plan named as a cross-check oracle. Writing `afm/cj/debug/perf/in_temp` / `in_alt` /
`in_weight` / `in_flaps` does **not** trigger a recomputation: `out_n1` holds whatever
the aircraft's own TOLD popup last calculated, so a swept comparison is not possible
through the debug datarefs. At the live ambient condition (RAT 19.9 degC, PA ~0 ft) the
aircraft reported `out_n1` = 97 against this plugin's 97.49; `out_n1` is an integer
dataref, so the two agree. Driving the aircraft's popup UI would be needed for a full
sweep; the cell-by-cell verification against the source charts above already covers the
table values.

## Independent check against the manufacturer's charts (2026-08-30)

Everything above is community work. The **Cessna Model 525 Operating Manual**
(525OMA-00, Section VII *Flight Planning and Performance*, Figures 7-6 to 7-10,
pp. 7-10 to 7-14) is the first *manufacturer* source to reach this repo, so the
tables were checked against it. It was supplied privately by a CJ1
owner-operator on the condition that it is not distributed, so it is not in this
repository; `scripts/digitize_om_charts.py` reads it from wherever you keep your
own copy.

**These are graphs, not tables.** Each chart is a family of straight rising
lines, one per pressure altitude, clipped from above by a configuration cap:

```text
N1(RAT, PA) = min(rise_PA(RAT), cap_config(RAT))
```

which is where the flat "caps" already visible in these CSVs come from — the
94.3 anti-ice ceiling that repeats across 1000-7000 ft is one cap line, read at
successive altitudes.

**Method.** Pages rendered at 400 dpi; pixels calibrated to chart units from the
printed x tick labels (only 0..+60 °C, since a leading minus drags a negative
label's centroid off its tick) and from a comb fitted to the major N1
gridlines. The two axes are fitted independently and must agree, because the
grid squares are square: they do, to within 0.5% on every page. Curves are
isolated by morphological opening, which removes the fine grid and leaves the
plotted strokes, then fitted by RANSAC and traced. Fitted lines were overlaid on
the scan and land dead-centre on the printed strokes, so the residual
disagreement below is not a digitising artefact.

**Result.** Figure 7-6 is a single chart titled "TAKEOFF/GO AROUND THRUST
SETTING" — structural confirmation that go-around and takeoff share one
schedule, which is the assumption behind the 2000-ft go-around correction above.
Against the shipped tables:

| Table | n | mean | sd | max abs |
| --- | --- | --- | --- | --- |
| `n1_takeoff.csv` | 64 | +0.12 | 0.52 | 0.77 |
| `n1_takeoff_ai.csv` | 40 | +0.27 | 0.40 | 0.93 |
| `n1_goaround.csv` | 64 | +0.06 | 0.52 | 0.77 |
| `n1_goaround_ai.csv` | 40 | +0.16 | 0.41 | 0.93 |

That is about what reading a photocopied graph supports, and it is *worse* than
the tables' own resolution: these CSVs are transcribed from printed numbers at
0.1 %N1, so **the charts corroborate them rather than replace them.** The check
rules out the failures that would matter — wrong schedule, wrong axis, wrong
variant, transposition — but cannot adjudicate at the tenth.

**Both flagged adjudications are confirmed.** Every one of the seven cells where
this repo departed from a printed value sits closer to the manufacturer's chart
than the value it rejected:

| Cell | Chart | Adopted | Rejected |
| --- | --- | --- | --- |
| GA 2000 ft, -20 °C | 92.6 | **92.9** | 91.7 |
| GA 2000 ft, -10 °C | 94.3 | **94.7** | 93.5 |
| GA 2000 ft, 0 °C | 96.0 | **96.5** | 95.3 |
| GA 2000 ft, 10 °C | 97.7 | **98.3** | 97.0 |
| TO 7000 ft, -10 °C black | 101.9 | **102.2** | 100.2 |
| TO 7000 ft, -25 °C red | 99.1 | **98.6** | 96.8 |
| TO 7000 ft, -20 °C red | 98.3 | **97.4** | 92.4 |

The 2000-ft go-around block and the 7000-ft "TO DO STILL" block were the two
regions the manual called out as carrying known uncertainty. They no longer do.

## What the Operating Manual has that these tables do not

Three schedules exist in the manufacturer's charts that no table here covers.
None is modelled yet; see [`../docs/CONTRACTS.md`](../docs/CONTRACTS.md).

- **Engine-only anti-ice.** Figures 7-8 and 7-10 both carry the note: "FOR ONLY
  ENGINE ANTI-ICE ON, REDUCE THE ANTI-ICE OFF N1 FROM FIGURE 7-7 [7-9 for
  cruise] BY 1% N1 EXCEPT THE RESULTING N1 NEED NOT BE LESS THAN THE ANTI-ICE ON
  - ALL N1 FROM THIS CHART." That is `max(dry - 1.0, wet)`, for climb and cruise
  only — the takeoff chart publishes no engine-only case.
- **ISA deviation at altitude.** Climb at or above 25,000 ft and cruise at or
  above 30,000 ft reduce N1 by 1.0 for ISA+11 to +20 °C and 2.0 for ISA+21 to
  +30 °C, nothing below ISA+11.
- **Environmental (ECS) bleed.** The climb and cruise anti-ice-off charts publish
  separate "ENVIRONMENTAL SYSTEMS - OFF" and "- ON" caps. The takeoff chart is
  drawn for environmental systems on only.

The charts also settle what "anti-ice on" means: Figures 7-8 and 7-10 are titled
"ANTI-ICE ON (ALL-ENGINE, WING, AND WINDSHIELD)", the same three systems the
flight manual supplement lists as W/S, ENG and WING.
