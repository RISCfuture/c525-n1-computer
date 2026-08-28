#!/usr/bin/env python3
"""Drive the plugin through its scenarios in a running sim and check the outputs.

Expected N1 values are computed here from the shipped CSVs by an independent
bilinear implementation, so a matching result cross-checks the C++ device.
"""

import csv
import pathlib
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from xp_udp import cmnd, get_drefs, set_dref

DATA = pathlib.Path(__file__).parent.parent / "data"
OFF, SELF_TEST, DASHES, RAT, TEMP_SET, N1, FAIL = range(7)
STATE_NAMES = {
    OFF: "Off",
    SELF_TEST: "888",
    DASHES: "Dashes",
    RAT: "RAT",
    TEMP_SET: "TempSet",
    N1: "N1",
    FAIL: "Fail",
}


def load(name):
    rows = list(csv.reader((DATA / name).open()))
    alts = [float(v) for v in rows[0][1:]]
    oats, grid = [], []
    for row in rows[1:]:
        if not row or not row[0].strip():
            continue
        oats.append(float(row[0]))
        grid.append([float(v) if v.strip() else None for v in row[1:]])
    return oats, alts, grid


def lookup(table, oat, alt):
    oats, alts, grid = table

    def bracket(axis, value, sea_level_datum=False):
        # Off-chart means no data. The one exception is pressure altitude below
        # the bottom line, which is the sea-level datum: negative PA is a
        # high-QNH artifact, not an off-chart condition.
        if value < axis[0]:
            if not sea_level_datum:
                return None
            value = axis[0]
        if value > axis[-1]:
            return None
        for i in range(len(axis) - 1):
            if axis[i] <= value <= axis[i + 1]:
                span = axis[i + 1] - axis[i]
                return i, i + 1, 0.0 if span == 0 else (value - axis[i]) / span
        return len(axis) - 1, len(axis) - 1, 0.0

    row, col = bracket(oats, oat), bracket(alts, alt, sea_level_datum=True)
    if row is None or col is None:
        return None
    r0, r1, fr = row
    c0, c1, fc = col
    corners = [grid[r][c] for r in (r0, r1) for c in (c0, c1)]
    if any(v is None for v in corners):
        return None
    top = grid[r0][c0] + (grid[r0][c1] - grid[r0][c0]) * fc
    bot = grid[r1][c0] + (grid[r1][c1] - grid[r1][c0]) * fc
    return top + (bot - top) * fr


TABLES = {
    n: load(f"n1_{n}.csv")
    for n in (
        "takeoff",
        "takeoff_ai",
        "goaround",
        "goaround_ai",
        "climb",
        "climb_ai",
        "cruise",
        "cruise_ai",
    )
}

# name, mode(0 CLB/1 TOGA/2 CRU), rat, pa, on_ground, anti_ice, powered, air_data_failed,
# expected table or state
SCENARIOS = [
    ("SL std day ground TO", 1, 15.0, 0, 1, 0, 1, 0, "takeoff"),
    ("Hot-high ground TO", 1, 35.0, 7000, 1, 0, 1, 0, "takeoff"),
    ("Cold ground TO", 1, -20.0, 0, 1, 0, 1, 0, "takeoff"),
    ("Anti-ice-on TO", 1, 0.0, 2000, 1, 2, 1, 0, "takeoff_ai"),
    ("Partial anti-ice TO", 1, 0.0, 2000, 1, 1, 1, 0, DASHES),
    ("CLB on ground", 0, 15.0, 0, 1, 0, 1, 0, DASHES),
    ("CRU on ground", 2, 15.0, 0, 1, 0, 1, 0, DASHES),
    ("Airborne climb", 0, -30.0, 25000, 0, 0, 1, 0, "climb"),
    ("Airborne cruise", 2, -45.0, 39000, 0, 0, 1, 0, "cruise"),
    ("Airborne climb AI", 0, -10.0, 15000, 0, 2, 1, 0, "climb_ai"),
    ("GA low", 1, 10.0, 3000, 0, 0, 1, 0, "goaround"),
    ("GA high (>15500)", 1, 0.0, 16500, 0, 0, 1, 0, DASHES),
    ("Air data failed TO", 1, 15.0, 0, 1, 0, 1, 1, FAIL),
    ("Air data failed cruise", 2, -40.0, 35000, 0, 0, 1, 1, FAIL),
    ("Off-chart PA on TO", 1, 15.0, 30000, 1, 0, 1, 0, DASHES),
    ("Off-chart cold RAT", 1, -40.0, 0, 1, 0, 1, 0, DASHES),
    ("Negative PA (high QNH)", 1, 15.0, -50, 1, 0, 1, 0, "takeoff"),
    ("FL410 cruise (top line)", 2, -45.0, 41000, 0, 0, 1, 0, "cruise"),
    ("Above FL410", 2, -45.0, 42000, 0, 0, 1, 0, DASHES),
    ("Unpowered", 1, 15.0, 0, 1, 0, 0, 0, OFF),
]


def set_mode(target):
    current = round(get_drefs(["sfn1/mode"]).get("sfn1/mode", 1))
    for _ in range(4):
        if current == target:
            return
        cmnd("sfn1/mode_up" if target > current else "sfn1/mode_down")
        time.sleep(0.25)
        current = round(get_drefs(["sfn1/mode"]).get("sfn1/mode", current))


def display_state():
    return round(get_drefs(["sfn1/display_state"]).get("sfn1/display_state", -1))


def breaker_steps():
    """An unsatisfactory self-test latches until the device is power-cycled, and
    the N1 IND breaker is how a pilot does that. Walks the sequence in the live
    sim and returns its (step, expected, actual) rows — the flat SCENARIOS table
    above is single-shot and cannot express a sequence."""
    steps = []

    def observe(name, want):
        steps.append((name, want, display_state()))

    set_dref("sfn1/test_override_rat_c", 15.0)
    set_dref("sfn1/test_override_pa_ft", 0)
    set_dref("sfn1/test_override_on_ground", 1)
    set_dref("sfn1/test_override_anti_ice", 0)
    set_dref("sfn1/test_override_powered", 0)
    cmnd("sfn1/breaker_reset")
    time.sleep(0.4)
    set_mode(1)

    # Powering up with the air data failed is an unsatisfactory self-test. The
    # latch is captured on the rising edge of power, and these are two separate
    # datagrams, so let the failure land first or the edge misses it.
    set_dref("sfn1/test_override_air_data_failed", 1)
    time.sleep(0.4)
    set_dref("sfn1/test_override_powered", 1)
    time.sleep(6.5)
    observe("latched blank", FAIL)

    # Recovering the air data alone does not clear the latch.
    set_dref("sfn1/test_override_air_data_failed", 0)
    time.sleep(1.0)
    observe("latched after recovery", FAIL)

    cmnd("sfn1/breaker_pull")
    time.sleep(0.8)
    observe("breaker pulled", OFF)

    cmnd("sfn1/breaker_reset")
    time.sleep(1.0)
    observe("self-test on reset", SELF_TEST)
    time.sleep(6.0)
    observe("recovered", N1)

    # Cycling the breaker with the air data still bad latches again: the
    # breaker restores power, not the air data source.
    set_dref("sfn1/test_override_air_data_failed", 1)
    cmnd("sfn1/breaker_pull")
    time.sleep(0.8)
    cmnd("sfn1/breaker_reset")
    time.sleep(6.5)
    observe("relatched on bad air data", FAIL)

    # Surviving the air data recovering is what separates a latched self-test
    # from a failure the device is merely reporting live.
    set_dref("sfn1/test_override_air_data_failed", 0)
    time.sleep(1.0)
    observe("relatch survives recovery", FAIL)

    # Leave the device healthy for whatever runs next.
    cmnd("sfn1/breaker_pull")
    time.sleep(0.8)
    cmnd("sfn1/breaker_reset")
    return steps


def report(name, exp_s, act_s, ok):
    print(f"{name:24s} {exp_s:>12s} {act_s:>12s}  {'PASS' if ok else 'FAIL'}")
    return not ok


def main():
    set_dref("sfn1/test_override_enable", 1)
    cmnd("sfn1/breaker_reset")  # an earlier run may have left it out
    failures = 0
    checks = len(SCENARIOS)
    print(f"{'scenario':24s} {'expect':>12s} {'actual':>12s}  result")
    for name, mode, rat, pa, ground, ai, powered, failed, want in SCENARIOS:
        set_dref("sfn1/test_override_rat_c", rat)
        set_dref("sfn1/test_override_pa_ft", pa)
        set_dref("sfn1/test_override_on_ground", ground)
        set_dref("sfn1/test_override_anti_ice", ai)
        set_dref("sfn1/test_override_powered", powered)
        set_dref("sfn1/test_override_air_data_failed", failed)
        time.sleep(0.4)
        set_mode(mode)
        time.sleep(6.5)  # let the 5 s self-test finish after a power change
        vals = get_drefs(["sfn1/target_n1", "sfn1/display_state"])
        state = round(vals.get("sfn1/display_state", -1))
        n1 = vals.get("sfn1/target_n1", -1)
        if isinstance(want, str):
            expected = lookup(TABLES[want], rat, pa)
            ok = state == N1 and expected is not None and abs(n1 - expected) <= 0.05
            exp_s, act_s = f"{expected:.2f}", f"{n1:.2f} {STATE_NAMES.get(state, '?')}"
        else:
            ok = state == want
            exp_s, act_s = STATE_NAMES[want], STATE_NAMES.get(state, str(state))
        failures += report(name, exp_s, act_s, ok)

    print("\nN1 IND circuit breaker")
    for name, want, got in breaker_steps():
        checks += 1
        failures += report(name, STATE_NAMES[want], STATE_NAMES.get(got, str(got)), got == want)

    set_dref("sfn1/test_override_air_data_failed", 0)
    set_dref("sfn1/test_override_enable", 0)
    print(f"\n{checks - failures}/{checks} checks passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
