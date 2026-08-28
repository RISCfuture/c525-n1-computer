# SafeFlightN1 — Internal contracts (fixed; all components conform)

## CSV table schema (`data/`)

Files: `n1_takeoff.csv`, `n1_takeoff_ai.csv`, `n1_goaround.csv`, `n1_goaround_ai.csv`,
`n1_climb.csv`, `n1_climb_ai.csv`, `n1_cruise.csv`, `n1_cruise_ai.csv`.
(`_ai` = anti-ice-on schedule.)

Format: comma-separated grid. Cell (0,0) is the literal string `oat_c\pa_ft`.
Header row: pressure altitudes in feet, ascending (e.g. `0,1000,...,7000` for TO/GA;
`0,5000,10000,15000,20000,25000,30000,35000,37000,39000,41000` for climb/cruise).
First column: OAT/RAT in °C, ascending (TO: −25…40; GA: −30…40; climb/cruise: −45…45 step 5).
Cells: N1 % with one decimal (e.g. `95.9`). **Empty cell = no charted data** (→ dashes).
No trailing commas. Unix newlines.

## `n1_tables` API (`src/n1_tables.h`) — pure C++, no XPLM

```cpp
namespace sfn1 {
class N1Table {
public:
    static std::optional<N1Table> loadFromCsv(const std::string& csvText);
    // Bilinear interpolation; nullopt when there is no charted data — an input
    // outside the grid axes, or any surrounding cell empty.
    std::optional<double> lookup(double oatC, double paFt) const;
};
}
```

## `n1_computer` API (`src/n1_computer.h`) — pure C++, no XPLM

```cpp
namespace sfn1 {
enum class Mode { Clb, ToGa, Cru };                      // knob detents, CLB left / TO-GA center / CRU right
enum class DisplayState { Off, SelfTest888, Dashes, Rat, TempSet, N1, Fail };
struct InputSnapshot {
    bool   powered;          // avionics bus energized
    bool   onGround;         // WOW
    double ratC;             // ram air temp °C
    double pressureAltFt;
    int    antiIce;          // 0 = off, 1 = partial, 2 = all bleed anti-ice on
    bool   knobPressed;
    bool   airDataFailed;    // air data feeding RAT/pressure altitude is invalid
};
struct Output {
    DisplayState state;
    double       value;      // N1 % / RAT °C / selected temp °C, per state
    Mode         mode;
};
class N1Computer {  // owns the Tables set; tick(input, dtSeconds) -> Output
public:
    void setMode(Mode);  void bumpTemp(double deltaC);   // press+rotate
    Output tick(const InputSnapshot&, double dtSeconds);
};
}
```

Behavior per plan: 888 self-test on power-up (default 5 s, constant in one place);
ground: TOGA = takeoff N1 (RAT or selected temp), CLB/CRU = dashes; air: CLB/CRU/GA
targets from actual RAT+PA, GA only ≤ 15,500 ft (above → dashes); anti-ice partial →
dashes; missing table data → dashes; selected temp applies on ground only, cleared on
liftoff; after landing, revert to 888 after 60 s.

Failures (FlightSafety CJ1 PTM Vol 2, pp. 16-167/16-169): "the display will blank for
any failure". `Fail` blanks the display while leaving it energized, so the unlit ghost
segments stay visible — `Off` renders nothing at all. Air data is invalid when any of
`rel_adc_comp`, `rel_adc_comp_2`, `rel_static`, `rel_static2` or `rel_tat_2` reads 6
(failed now; 1–5 only *arm* a failure). Standby sources are excluded — they feed the
standby instruments, not the air data bus. A failure present at power-up is an
unsatisfactory self-test: the display blanks instead of showing 888 and latches until
the next power cycle; a failure arising later clears when the source recovers.
Out-of-chart inputs dash rather than clamping, which also reproduces the PTM's "at
maximum cruising altitude (FL 410), display of N1 may be intermittent" — FL410 sits
exactly on the cruise table's upper edge.

Clearing a latched self-test needs a power cycle, which the **N1 IND circuit breaker**
provides: pulling it opens the device's power feed (`Off`), and resetting it re-runs the
self-test. Resetting with the air data still failed simply latches again — the breaker
restores power, not the air data source. The device cannot tell an open breaker from a
dead bus, so the breaker is folded into `powered` in `sim_inputs.cpp` rather than modelled
as device state. A power cycle spanning a landing does not arm the 60 s revert to "888":
the device was unpowered when the airplane touched down, so it never saw the landing. That
holds for any power interruption, not only the breaker.

Sourcing: the PTM's SafeFlight section names no breaker for the unit, but
its circuit-breaker panel diagrams (Vol 2, Figures 2-7/2-8, pp. 2-7/2-8) show a 5 A "N1
IND" breaker in the AVIONICS DC section of the right panel, fed from the right feed
extension bus. It is the only candidate: the standby N1 indicators have their own "L/R
STBY N1" breakers on the emergency bus (Table 2-1, p. 2-5), and primary N1 reaches the MFD
through the DCU/EDC breakers. **The breaker, its label, its section and its bus are all
verbatim from the diagrams; only the link to this box is inference by elimination.** That
bus is `afm/cj/f/elec/V_bus_av_r_feed_ext` — one of the two the plugin already reads for
power, so the sourcing and the power model agree. Breaker position is not persisted; it
returns to "in" whenever an aircraft loads.

## Datarefs published (all read-only except overrides)

`sfn1/target_n1` f (−1 when no valid target) · `sfn1/mode` i (0 CLB, 1 TOGA, 2 CRU) ·
`sfn1/display_state` i (0 Off, 1 SelfTest888, 2 Dashes, 3 Rat, 4 TempSet, 5 N1, 6 Fail) ·
`sfn1/selected_temp_c` f (−999 when no temp is dialed) · `sfn1/rat_c` f ·
`sfn1/air_data_failed` i (1 when an air data source is failed) ·
`sfn1/breaker_pulled` i (writable; 1 while the N1 IND breaker is pulled)
Debug overrides (always compiled, default off): `sfn1/test_override_enable` i,
`sfn1/test_override_rat_c` f, `sfn1/test_override_pa_ft` f, `sfn1/test_override_on_ground` i,
`sfn1/test_override_anti_ice` i (0/1/2), `sfn1/test_override_powered` i,
`sfn1/test_override_air_data_failed` i.

## Commands

`sfn1/toggle_window`, `sfn1/mode_up`, `sfn1/mode_down`, `sfn1/knob_press` (held),
`sfn1/temp_up`, `sfn1/temp_down`, `sfn1/breaker_pull`, `sfn1/breaker_reset`,
`sfn1/breaker_toggle`. The plugin menu carries one breaker item, backed by
`breaker_toggle`, whose label follows the breaker's position (Pull ↔ Reset).

## Faceplate layout contract (`assets/layout.json`)

Normalized coordinates (0–1 of faceplate.png width/height):

```json
{ "png_size": [W, H],
  "display_window": {"x":…,"y":…,"w":…,"h":…},
  "knob": {"cx":…,"cy":…,"r":…, "angles_deg": {"CLB":…, "TOGA":0, "CRU":…}},
  "digit_cells": 3, "decimal_after": 2 }
```

`window.cpp` consumes this — art and code stay decoupled.

## Aircraft gate

The device is STC'd equipment on the Citation 525, so the plugin runs only while
`CJ525.acf` is the user aircraft and every `afm/cj/*` handle has resolved. With any
other aircraft loaded it publishes an inert state (`display_state` 0, `target_n1` −1,
`selected_temp_c` −999), keeps its window closed, and greys out its menu items.
Setting `sfn1/test_override_enable` bypasses the gate so the host and scenario
harnesses can drive the logic without the aircraft.

Handle resolution is all-or-nothing and retries for as long as the CJ525 stays
loaded: a partially resolved gate would read anti-ice as off and publish the dry
schedule, which looks healthy while showing the wrong target.

The plugin installs to `Resources/plugins/` rather than the aircraft's own
`plugins/` folder. X-Plane would gate an aircraft-folder plugin natively, but the
CJ525 is payware that self-updates, and an update replacing the aircraft folder
would silently delete the plugin.

## Build / style

C++20; no exceptions across the XPLM boundary; small fluent functions; doc comments
(Doxygen style) on public APIs only. XPLM420, built for `mac_x64` (universal
arm64+x86_64), `win_x64` (MSVC) and `lin_x64` (GCC, runtime linked statically).
Formatting and lint rules live in `.clang-format`, `.clang-tidy` and `ruff.toml`.
Host tests: `tests/run_tests.sh` compiles logic + tests directly with `$CXX` (no sim).

Paths from `XPLMGetPluginInfo` come back with native separators, so anything that
splits one goes through `pluginDir()` in `src/plugin_paths.h` rather than assuming
'/'.
