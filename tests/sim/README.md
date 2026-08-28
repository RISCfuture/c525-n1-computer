# SafeFlightN1 in-sim test harness

`sfn1_test_harness.lua` is a **dev-only** FlyWithLua NG+ (2.8.16+) script that
exercises the SafeFlightN1 plugin inside X-Plane 12. It is never shipped with
the plugin.

It works by writing the plugin's `sfn1/test_override_*` datarefs (with
`sfn1/test_override_enable = 1`), so scenarios set RAT, pressure altitude,
on-ground, anti-ice, and power directly instead of fighting X-Plane's weather.
For each scenario it computes the expected N1 **independently** — it parses the
CSV tables in `data/` and bilinearly interpolates in Lua, a deliberate second
implementation that cross-checks the plugin's C++ — then compares against the
live `sfn1/target_n1` and `sfn1/display_state`:

- **PASS** — value within ±0.05 %N1 (states must match exactly)
- **WAIT** — settling, or the 888 self-test is running
- **FAIL** — mismatch after the settle window
- **NOTBL** — the needed CSV table is missing (state-only scenarios still work)

## Requirements

- X-Plane 12 with the TorqueSim CitationJet 525 loaded (the script gates on
  `PLANE_ICAO == "C525"` and does nothing for other aircraft)
- FlyWithLua NG+ 2.8.16 or later
- The SafeFlightN1 plugin installed (the harness waits and retries until the
  `sfn1/*` datarefs appear)
- The repo's `data/*.csv` tables; their absolute path is hardcoded as
  the installed plugin's `data/` folder, or `SFN1_DATA_DIR` if that is set

## Install

Symlink (recommended — edits to the repo copy take effect on script reload):

```sh
ln -s "$PWD/tests/sim/sfn1_test_harness.lua" \
      "$XPLANE_DIR/Resources/plugins/FlyWithLua/Scripts/"
```

Or copy the file into that `Scripts/` directory instead.

Then start X-Plane (or Plugins → FlyWithLua → Reload all Lua script files).

## Usage

With the C525 loaded, the "SFN1 Test Harness" floating window opens
automatically; reopen it any time via Plugins → FlyWithLua → FlyWithLua Macros
→ "SFN1 test harness: window".

- Click a scenario button to force the sim into that state. The row shows the
  expected value/state, the live plugin output, and PASS/WAIT/FAIL.
- Scenarios that need a knob mode (CLB/TOGA/CRU) drive it via the
  `sfn1/mode_up` / `sfn1/mode_down` commands, one step per frame.
- **LIVE (overrides off)** sets `sfn1/test_override_enable = 0` so the plugin
  follows the real sim state again.
- The bottom sections show the raw sim inputs (RAT/OAT, pressure altitude,
  on-ground, the CJ anti-ice switches) and the aircraft's own
  `afm/cj/debug/perf/out_n1` oracle for eyeball comparison.
- "Power cycle (self-test)" cuts override power for one second, restores it,
  and passes once the 888 self-test is observed.

Missing tables are re-scanned every 5 seconds, so you can generate `data/`
while the sim is running.

## Removal

Delete the symlink (or copied file) from
`.../FlyWithLua/Scripts/sfn1_test_harness.lua` and reload FlyWithLua scripts.
The harness leaves no state behind; if you removed it while overrides were
active, clear them by setting `sfn1/test_override_enable` to `0` in DataRefTool
or by restarting the sim.
