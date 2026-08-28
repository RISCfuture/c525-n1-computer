-- SafeFlightN1 test harness (dev-only; never shipped).
--
-- FlyWithLua NG+ 2.8.16 script: drives the SafeFlightN1 plugin through canned
-- scenarios via its sfn1/test_override_* datarefs and cross-checks the plugin's
-- sfn1/target_n1 against an independent Lua reimplementation of the CSV table
-- lookup (bilinear interpolation), so the two implementations verify each other.
--
-- Install: copy or symlink into "<X-Plane 12>/Resources/plugins/FlyWithLua/Scripts/".
-- See tests/sim/README.md.

-- Where the CSV schedules are read from, relative to the X-Plane folder that
-- FlyWithLua runs in. The default is the installed plugin's own copy, which is
-- also the repository's when dist/ is symlinked in for development. Set
-- SFN1_DATA_DIR to read them from somewhere else.
local DATA_DIR = os.getenv("SFN1_DATA_DIR") or "Resources/plugins/SafeFlightN1/data"

local under_fwl = type(logMsg) == "function"
if under_fwl and not SUPPORTS_FLOATING_WINDOWS then
    logMsg("SFN1 harness: FlyWithLua too old (no floating windows), not loading")
    return
end
if under_fwl and PLANE_ICAO ~= "C525" then
    logMsg("SFN1 harness: aircraft is not C525, not loading")
    return
end

--------------------------------------------------------------------------------
-- Independent N1 table engine (deliberately a second implementation of the
-- plugin's C++ N1Table, per docs/CONTRACTS.md: clamp to grid edges, bilinear
-- interpolation, any empty surrounding cell -> no value).
--------------------------------------------------------------------------------

local TABLE_NAMES = {
    "n1_takeoff", "n1_takeoff_ai", "n1_goaround", "n1_goaround_ai",
    "n1_climb", "n1_climb_ai", "n1_cruise", "n1_cruise_ai",
}

local function split_csv(line)
    local fields = {}
    for field in (line .. ","):gmatch("([^,]*),") do
        fields[#fields + 1] = field
    end
    return fields
end

local function non_blank_lines(text)
    local lines = {}
    for raw in text:gmatch("[^\n]+") do
        local line = raw:gsub("\r", "")
        if line:match("%S") then lines[#lines + 1] = line end
    end
    return lines
end

local function parse_grid(rows)
    local header = split_csv(rows[1])
    local pas, oats, cells = {}, {}, {}
    for c = 2, #header do
        pas[c - 1] = tonumber(header[c])
        if not pas[c - 1] then return nil, "bad header cell " .. c end
    end
    for r = 2, #rows do
        local row = split_csv(rows[r])
        oats[r - 1] = tonumber(row[1])
        if not oats[r - 1] then return nil, "bad OAT in row " .. r end
        local row_cells = {}
        for c = 2, #header do
            row_cells[c - 1] = tonumber(row[c])
        end
        cells[r - 1] = row_cells
    end
    return { pas = pas, oats = oats, cells = cells }
end

local function parse_csv(path)
    local file = io.open(path, "r")
    if not file then return nil, "not found" end
    local text = file:read("*a")
    file:close()
    local rows = non_blank_lines(text)
    if #rows < 2 then return nil, "too few rows" end
    return parse_grid(rows)
end

local function bracket(grid, x)
    if x <= grid[1] then return 1, 1, 0 end
    local n = #grid
    if x >= grid[n] then return n, n, 0 end
    for i = 1, n - 1 do
        if x <= grid[i + 1] then
            return i, i + 1, (x - grid[i]) / (grid[i + 1] - grid[i])
        end
    end
end

local function lookup(tbl, oat_c, pa_ft)
    local i0, i1, ti = bracket(tbl.oats, oat_c)
    local j0, j1, tj = bracket(tbl.pas, pa_ft)
    local v00, v01 = tbl.cells[i0][j0], tbl.cells[i0][j1]
    local v10, v11 = tbl.cells[i1][j0], tbl.cells[i1][j1]
    if not (v00 and v01 and v10 and v11) then return nil end
    local low = v00 + (v01 - v00) * tj
    local high = v10 + (v11 - v10) * tj
    return low + (high - low) * ti
end

if not under_fwl then
    -- Loaded by a plain Lua interpreter (host-side check of this file's own
    -- table engine); expose the pure functions and skip all sim wiring.
    SFN1H_INTERNALS = { parse_csv = parse_csv, lookup = lookup, bracket = bracket }
    return
end

--------------------------------------------------------------------------------
-- Harness state (single namespaced global: FWL scripts share one Lua state)
--------------------------------------------------------------------------------

SFN1H = {
    wnd = nil,
    refs = {},           -- dataref name -> userdata ref
    refs_ok = false,
    cmds_ok = false,
    tables = {},         -- table name -> parsed grid
    table_errs = {},     -- table name -> error string
    tables_last_try = -1e9,
    active = nil,        -- index into SCENARIOS
    applied_at = 0,
    settle_until = 0,
    pending_mode = nil,
    mode_attempts = 0,
    pc = nil,            -- power-cycle phase state
    time_ref = XPLMFindDataRef("sim/time/total_running_time_sec"),
}

local STATE_NAMES = { [0] = "Off", [1] = "888", [2] = "Dashes", [3] = "RAT", [4] = "TempSet", [5] = "N1", [6] = "Fail" }
local MODE_NAMES = { [0] = "CLB", [1] = "TOGA", [2] = "CRU" }
local MODE = { CLB = 0, TOGA = 1, CRU = 2 }
local STATE = { OFF = 0, SELFTEST = 1, DASHES = 2, N1 = 5, FAIL = 6 }

local N1_TOLERANCE = 0.05
local SELF_TEST_SECS = 5
local POWER_OFF_SECS = 1.0
local SETTLE_SECS = 1.5
local MODE_NUDGE_LIMIT = 12

local COL_OK = 0xFF00FF00
local COL_FAIL = 0xFF0000FF
local COL_WAIT = 0xFF00FFFF
local COL_DIM = 0xFF888888
local COL_HEAD = 0xFFFFCC66

local SFN1_FLOAT_REFS = {
    "sfn1/target_n1", "sfn1/selected_temp_c", "sfn1/rat_c",
    "sfn1/test_override_rat_c", "sfn1/test_override_pa_ft",
}
local SFN1_INT_REFS = {
    "sfn1/mode", "sfn1/display_state",
    "sfn1/test_override_enable", "sfn1/test_override_on_ground",
    "sfn1/test_override_anti_ice", "sfn1/test_override_powered",
    "sfn1/test_override_air_data_failed", "sfn1/air_data_failed",
}
local SFN1_COMMANDS = { "sfn1/mode_up", "sfn1/mode_down" }

local function now()
    if SFN1H.time_ref then return XPLMGetDataf(SFN1H.time_ref) end
    return os.clock()
end

local function getf(name)
    local ref = SFN1H.refs[name]
    return ref and XPLMGetDataf(ref)
end

local function geti(name)
    local ref = SFN1H.refs[name]
    return ref and XPLMGetDatai(ref)
end

local function setf(name, value)
    local ref = SFN1H.refs[name]
    if ref then XPLMSetDataf(ref, value) end
end

local function seti(name, value)
    local ref = SFN1H.refs[name]
    if ref then XPLMSetDatai(ref, value) end
end

--------------------------------------------------------------------------------
-- Retrying resolution of the plugin's datarefs and commands
-- (the plugin may register after this script loads; WaitForCommandToBeCreated
-- pattern from the shipped FWL demos)
--------------------------------------------------------------------------------

local function resolve_named_refs(names)
    local all_found = true
    for _, name in ipairs(names) do
        if not SFN1H.refs[name] then
            SFN1H.refs[name] = XPLMFindDataRef(name)
            if not SFN1H.refs[name] then all_found = false end
        end
    end
    return all_found
end

local function resolve_sfn1_refs()
    if SFN1H.refs_ok then return end
    local floats = resolve_named_refs(SFN1_FLOAT_REFS)
    local ints = resolve_named_refs(SFN1_INT_REFS)
    local cmds = true
    for _, name in ipairs(SFN1_COMMANDS) do
        if XPLMFindCommand(name) == nil then cmds = false end
    end
    SFN1H.cmds_ok = cmds
    SFN1H.refs_ok = floats and ints and cmds
    if SFN1H.refs_ok then logMsg("SFN1 harness: all sfn1/* datarefs and commands resolved") end
end

local function load_missing_tables()
    for _, name in ipairs(TABLE_NAMES) do
        if not SFN1H.tables[name] then
            local tbl, err = parse_csv(DATA_DIR .. "/" .. name .. ".csv")
            SFN1H.tables[name] = tbl
            SFN1H.table_errs[name] = err
        end
    end
end

local function retry_tables_throttled()
    if now() - SFN1H.tables_last_try < 5 then return end
    SFN1H.tables_last_try = now()
    load_missing_tables()
end

local function loaded_table_count()
    local count = 0
    for _, name in ipairs(TABLE_NAMES) do
        if SFN1H.tables[name] then count = count + 1 end
    end
    return count
end

--------------------------------------------------------------------------------
-- Scenarios
--------------------------------------------------------------------------------

-- ov: values written into sfn1/test_override_* (enable is always set to 1).
-- mode: knob detent driven via sfn1/mode_up / mode_down commands.
-- expect: kind "n1" (value from `table` at oat/pa), "state", or "selftest".
local SCENARIOS = {
    { name = "SL std day ground TO", ov = { rat_c = 15, pa_ft = 0, on_ground = 1, anti_ice = 0, powered = 1 },
      mode = MODE.TOGA, expect = { kind = "n1", table = "n1_takeoff", oat = 15, pa = 0 } },
    { name = "Hot-high ground TO", ov = { rat_c = 35, pa_ft = 7000, on_ground = 1, anti_ice = 0, powered = 1 },
      mode = MODE.TOGA, expect = { kind = "n1", table = "n1_takeoff", oat = 35, pa = 7000 } },
    { name = "Cold ground TO", ov = { rat_c = -20, pa_ft = 0, on_ground = 1, anti_ice = 0, powered = 1 },
      mode = MODE.TOGA, expect = { kind = "n1", table = "n1_takeoff", oat = -20, pa = 0 } },
    { name = "Anti-ice-on TO", ov = { rat_c = 0, pa_ft = 2000, on_ground = 1, anti_ice = 2, powered = 1 },
      mode = MODE.TOGA, expect = { kind = "n1", table = "n1_takeoff_ai", oat = 0, pa = 2000 } },
    { name = "Partial anti-ice TO", ov = { rat_c = 0, pa_ft = 2000, on_ground = 1, anti_ice = 1, powered = 1 },
      mode = MODE.TOGA, expect = { kind = "state", state = STATE.DASHES } },
    { name = "CLB on ground", ov = { rat_c = 15, pa_ft = 0, on_ground = 1, anti_ice = 0, powered = 1 },
      mode = MODE.CLB, expect = { kind = "state", state = STATE.DASHES } },
    { name = "Airborne climb", ov = { rat_c = -30, pa_ft = 25000, on_ground = 0, anti_ice = 0, powered = 1 },
      mode = MODE.CLB, expect = { kind = "n1", table = "n1_climb", oat = -30, pa = 25000 } },
    { name = "Airborne cruise", ov = { rat_c = -45, pa_ft = 39000, on_ground = 0, anti_ice = 0, powered = 1 },
      mode = MODE.CRU, expect = { kind = "n1", table = "n1_cruise", oat = -45, pa = 39000 } },
    { name = "GA low", ov = { rat_c = 10, pa_ft = 3000, on_ground = 0, anti_ice = 0, powered = 1 },
      mode = MODE.TOGA, expect = { kind = "n1", table = "n1_goaround", oat = 10, pa = 3000 } },
    { name = "GA high (>15,500 ft)", ov = { rat_c = 0, pa_ft = 16500, on_ground = 0, anti_ice = 0, powered = 1 },
      mode = MODE.TOGA, expect = { kind = "state", state = STATE.DASHES } },
    { name = "Unpowered", ov = { rat_c = 15, pa_ft = 0, on_ground = 1, anti_ice = 0, powered = 0 },
      expect = { kind = "state", state = STATE.OFF } },
    { name = "Power cycle (self-test)", ov = { rat_c = 15, pa_ft = 0, on_ground = 1, anti_ice = 0, powered = 0 },
      expect = { kind = "selftest" } },

    -- Failure and off-chart behaviour.
    { name = "Air data failed (ground TO)",
      ov = { rat_c = 15, pa_ft = 0, on_ground = 1, anti_ice = 0, powered = 1, air_data_failed = 1 },
      mode = MODE.TOGA, expect = { kind = "state", state = STATE.FAIL } },
    { name = "Air data failed (cruise)",
      ov = { rat_c = -40, pa_ft = 35000, on_ground = 0, anti_ice = 0, powered = 1, air_data_failed = 1 },
      mode = MODE.CRU, expect = { kind = "state", state = STATE.FAIL } },
    { name = "Off-chart PA on takeoff chart",
      ov = { rat_c = 15, pa_ft = 30000, on_ground = 1, anti_ice = 0, powered = 1 },
      mode = MODE.TOGA, expect = { kind = "state", state = STATE.DASHES } },
    { name = "Off-chart cold RAT",
      ov = { rat_c = -40, pa_ft = 0, on_ground = 1, anti_ice = 0, powered = 1 },
      mode = MODE.TOGA, expect = { kind = "state", state = STATE.DASHES } },
    { name = "Negative PA (high QNH)",
      ov = { rat_c = 15, pa_ft = -50, on_ground = 1, anti_ice = 0, powered = 1 },
      mode = MODE.TOGA, expect = { kind = "n1", table = "n1_takeoff", oat = 15, pa = 0 } },
    { name = "FL410 cruise (top chart line)",
      ov = { rat_c = -45, pa_ft = 41000, on_ground = 0, anti_ice = 0, powered = 1 },
      mode = MODE.CRU, expect = { kind = "n1", table = "n1_cruise", oat = -45, pa = 41000 } },
    { name = "Above FL410 (off chart)",
      ov = { rat_c = -45, pa_ft = 42000, on_ground = 0, anti_ice = 0, powered = 1 },
      mode = MODE.CRU, expect = { kind = "state", state = STATE.DASHES } },

    -- Real flight: overrides off, aircraft actually placed at altitude, so the
    -- device runs on live air data. Expectation is a state rather than a value
    -- because the sim's RAT settles on its own.
    { name = "LIVE: FL410 cruise, AP on", live = true, place = { alt_ft = 41000, tas_kt = 430 },
      ov = { rat_c = 0, pa_ft = 0, on_ground = 0, anti_ice = 0, powered = 1 },
      mode = MODE.CRU, expect = { kind = "state", state = STATE.N1 } },
    { name = "LIVE: FL200 climb, AP on", live = true, place = { alt_ft = 20000, tas_kt = 300 },
      ov = { rat_c = 0, pa_ft = 0, on_ground = 0, anti_ice = 0, powered = 1 },
      mode = MODE.CLB, expect = { kind = "state", state = STATE.N1 } },
}

-- Places the aircraft in level flight at a given altitude and true airspeed.
-- Altitude is applied as a delta on local_y against the current MSL elevation,
-- which avoids needing a world-to-local conversion. Used by scenarios that
-- exercise the real air data path rather than the override datarefs.
local function place_in_flight(alt_ft, tas_kt)
    -- Resolved directly rather than through raw_ref(), which is declared later
    -- in this file; placement is infrequent, so caching buys nothing.
    local function ref(name) return XPLMFindDataRef(name) end

    local y_ref = ref("sim/flightmodel/position/local_y")
    local elevation = ref("sim/flightmodel/position/elevation")
    if not (y_ref and elevation) then
        logMsg("SFN1 harness: cannot place aircraft, position datarefs missing")
        return false
    end
    local target_m = alt_ft * 0.3048
    XPLMSetDatad(y_ref, XPLMGetDatad(y_ref) + (target_m - XPLMGetDatad(elevation)))

    local psi = ref("sim/flightmodel/position/psi")
    local heading = psi and math.rad(XPLMGetDataf(psi)) or 0
    local speed_ms = tas_kt * 0.514444
    -- OpenGL axes: +x east, +y up, -z north.
    local vx, vy, vz = ref("sim/flightmodel/position/local_vx"),
                       ref("sim/flightmodel/position/local_vy"),
                       ref("sim/flightmodel/position/local_vz")
    if vx then XPLMSetDataf(vx, speed_ms * math.sin(heading)) end
    if vy then XPLMSetDataf(vy, 0) end
    if vz then XPLMSetDataf(vz, -speed_ms * math.cos(heading)) end

    for _, axis in ipairs({ "sim/flightmodel/position/theta", "sim/flightmodel/position/phi" }) do
        local axis_ref = ref(axis)
        if axis_ref then XPLMSetDataf(axis_ref, 0) end
    end

    local throttle = ref("sim/cockpit2/engine/actuators/throttle_ratio_all")
    if throttle then XPLMSetDataf(throttle, 0.9) end

    local ap_alt = ref("sim/cockpit2/autopilot/altitude_hold_ft")
    if ap_alt then XPLMSetDataf(ap_alt, alt_ft) end
    local ap_mode = ref("sim/cockpit2/autopilot/autopilot_mode")
    if ap_mode then XPLMSetDatai(ap_mode, 2) end

    logMsg(string.format("SFN1 harness: placed at %.0f ft / %.0f kt TAS", alt_ft, tas_kt))
    return true
end

local function apply_scenario(index)
    if not SFN1H.refs_ok then return end
    local s = SCENARIOS[index]
    setf("sfn1/test_override_rat_c", s.ov.rat_c)
    setf("sfn1/test_override_pa_ft", s.ov.pa_ft)
    seti("sfn1/test_override_on_ground", s.ov.on_ground)
    seti("sfn1/test_override_anti_ice", s.ov.anti_ice)
    seti("sfn1/test_override_powered", s.ov.powered)
    seti("sfn1/test_override_air_data_failed", s.ov.air_data_failed or 0)
    seti("sfn1/test_override_enable", s.live and 0 or 1)
    SFN1H.active = index
    SFN1H.applied_at = now()
    SFN1H.settle_until = SFN1H.applied_at + SETTLE_SECS
    SFN1H.pending_mode = s.mode
    SFN1H.mode_attempts = 0
    SFN1H.pc = s.expect.kind == "selftest" and { phase = 1, t0 = SFN1H.applied_at } or nil
    if s.place then place_in_flight(s.place.alt_ft, s.place.tas_kt) end
    logMsg("SFN1 harness: applied scenario \"" .. s.name .. "\"")
end

local function go_live()
    if not SFN1H.refs_ok then return end
    seti("sfn1/test_override_enable", 0)
    SFN1H.active = nil
    SFN1H.pending_mode = nil
    SFN1H.pc = nil
    logMsg("SFN1 harness: overrides off (LIVE)")
end

--------------------------------------------------------------------------------
-- Expected values and PASS/FAIL evaluation
--------------------------------------------------------------------------------

local function expected_n1(expect)
    local tbl = SFN1H.tables[expect.table]
    if not tbl then return nil, "table " .. expect.table .. " not loaded" end
    return lookup(tbl, expect.oat, expect.pa)
end

local function expected_text(s)
    local expect = s.expect
    if expect.kind == "state" then return "expect " .. STATE_NAMES[expect.state] end
    if expect.kind == "selftest" then return "expect 888 after power restore" end
    local value, err = expected_n1(expect)
    if err then return "expect N1 (" .. err .. ")" end
    if not value then return "expect Dashes (no charted data)" end
    return string.format("expect N1 %.1f", value)
end

local function live_summary()
    local state = geti("sfn1/display_state")
    local n1 = getf("sfn1/target_n1")
    return string.format("live: state=%s target_n1=%.1f", STATE_NAMES[state] or tostring(state), n1)
end

local function evaluate_selftest()
    local pc = SFN1H.pc
    if not pc then return "WAIT", "no phase state" end
    if pc.phase == 1 then return "WAIT", "power off; restoring shortly" end
    if pc.seen888 then return "PASS", "888 self-test observed" end
    if now() < pc.t_on + SELF_TEST_SECS - 0.5 then return "WAIT", "watching for 888" end
    return "FAIL", "888 never displayed after power restore"
end

local function evaluate_state(expect_state)
    local state = geti("sfn1/display_state")
    if state == expect_state then return "PASS", live_summary() end
    if state == STATE.SELFTEST and expect_state ~= STATE.SELFTEST then
        return "WAIT", "self-test running"
    end
    if now() < SFN1H.settle_until then return "WAIT", "settling" end
    return "FAIL", live_summary()
end

local function evaluate_n1(expect)
    local value, err = expected_n1(expect)
    if err then return "NOTBL", "tables not found: " .. err end
    if not value then return evaluate_state(STATE.DASHES) end
    local state = geti("sfn1/display_state")
    local n1 = getf("sfn1/target_n1")
    if state == STATE.N1 and math.abs(n1 - value) <= N1_TOLERANCE then
        return "PASS", string.format("delta %+.2f", n1 - value)
    end
    if state == STATE.SELFTEST then return "WAIT", "self-test running" end
    if now() < SFN1H.settle_until then return "WAIT", "settling" end
    return "FAIL", live_summary()
end

local function evaluate(s)
    if not SFN1H.refs_ok then return "WAIT", "resolving sfn1/* datarefs" end
    if s.expect.kind == "selftest" then return evaluate_selftest() end
    if s.expect.kind == "state" then return evaluate_state(s.expect.state) end
    return evaluate_n1(s.expect)
end

--------------------------------------------------------------------------------
-- Per-frame work: mode-knob nudging and the power-cycle sequence
--------------------------------------------------------------------------------

local function nudge_mode()
    if not SFN1H.pending_mode then return end
    local mode = geti("sfn1/mode")
    if mode == SFN1H.pending_mode then
        SFN1H.pending_mode = nil
        return
    end
    if SFN1H.mode_attempts >= MODE_NUDGE_LIMIT then return end
    SFN1H.mode_attempts = SFN1H.mode_attempts + 1
    command_once(mode < SFN1H.pending_mode and "sfn1/mode_up" or "sfn1/mode_down")
end

local function run_power_cycle()
    local pc = SFN1H.pc
    if not pc then return end
    if pc.phase == 1 and now() >= pc.t0 + POWER_OFF_SECS then
        seti("sfn1/test_override_powered", 1)
        pc.phase = 2
        pc.t_on = now()
    elseif pc.phase == 2 and geti("sfn1/display_state") == STATE.SELFTEST then
        pc.seen888 = true
    end
end

function sfn1h_frame()
    if not SFN1H.refs_ok then return end
    nudge_mode()
    run_power_cycle()
end

--------------------------------------------------------------------------------
-- Raw sim inputs shown for eyeball comparison (all optional; "--" if absent)
--------------------------------------------------------------------------------

local RAW_INPUTS = {
    { label = "RAT / LE temp degC", dr = "sim/cockpit2/temperature/outside_air_LE_temp_degc", kind = "f", fmt = "%.1f" },
    { label = "OAT degC", dr = "sim/cockpit2/temperature/outside_air_temp_degc", kind = "f", fmt = "%.1f" },
    { label = "Pressure alt ft", dr = "sim/flightmodel2/position/pressure_altitude", kind = "f", fmt = "%.0f" },
    { label = "On ground (any)", dr = "sim/flightmodel/failures/onground_any", kind = "i" },
    { label = "CJ ice bleed sw", dr = "afm/cj/switch_panel/ice_bleed", kind = "i" },
    { label = "CJ wing L / R sw", dr = "afm/cj/switch_panel/ice_wing_l", dr2 = "afm/cj/switch_panel/ice_wing_r", kind = "i" },
    { label = "CJ tail deice sw", dr = "afm/cj/switch_panel/ice_tail", kind = "i" },
    { label = "XP inlet heat", dr = "sim/cockpit/switches/anti_ice_inlet_heat", kind = "i" },
    { label = "ADC 1 failure", dr = "sim/operation/failures/rel_adc_comp", kind = "i" },
    { label = "Static 1 failure", dr = "sim/operation/failures/rel_static", kind = "i" },
    { label = "TAT probe failure", dr = "sim/operation/failures/rel_tat_2", kind = "i" },
    { label = "sfn1 air data failed", dr = "sfn1/air_data_failed", kind = "i" },
    { label = "Oracle out_n1", dr = "afm/cj/debug/perf/out_n1", kind = "i" },
    { label = "Oracle out_VALID", dr = "afm/cj/debug/perf/out_VALID", kind = "i" },
}

local function raw_ref(name)
    local cached = SFN1H.refs[name]
    if cached ~= nil then return cached or nil end
    local ref = XPLMFindDataRef(name)
    SFN1H.refs[name] = ref or false
    return ref
end

local function retry_missing_raw_refs()
    for _, input in ipairs(RAW_INPUTS) do
        if SFN1H.refs[input.dr] == false then SFN1H.refs[input.dr] = nil end
        if input.dr2 and SFN1H.refs[input.dr2] == false then SFN1H.refs[input.dr2] = nil end
    end
end

function sfn1h_often()
    resolve_sfn1_refs()
    retry_missing_raw_refs()
    if loaded_table_count() < #TABLE_NAMES then retry_tables_throttled() end
end

local function raw_value_text(input, name)
    local ref = name and raw_ref(name)
    if not ref then return "--" end
    if input.kind == "f" then return string.format(input.fmt, XPLMGetDataf(ref)) end
    return tostring(XPLMGetDatai(ref))
end

--------------------------------------------------------------------------------
-- imgui window
--------------------------------------------------------------------------------

local function colored_text(color, text)
    imgui.PushStyleColor(imgui.constant.Col.Text, color)
    imgui.TextUnformatted(text)
    imgui.PopStyleColor()
end

local function result_color(status)
    if status == "PASS" then return COL_OK end
    if status == "FAIL" then return COL_FAIL end
    if status == "NOTBL" then return COL_DIM end
    return COL_WAIT
end

local function draw_header()
    if SFN1H.refs_ok then
        colored_text(COL_OK, "SafeFlightN1 plugin: connected")
    else
        colored_text(COL_WAIT, "SafeFlightN1 plugin: waiting for sfn1/* datarefs...")
    end
    local count = loaded_table_count()
    if count == #TABLE_NAMES then
        colored_text(COL_OK, string.format("N1 tables: %d/%d loaded from %s", count, #TABLE_NAMES, DATA_DIR))
    else
        colored_text(COL_WAIT, string.format("N1 tables: %d/%d loaded (tables not found under %s; retrying)",
            count, #TABLE_NAMES, DATA_DIR))
    end
end

local function draw_scenario_row(index, s)
    imgui.PushID(index)
    if imgui.Button(s.name) then apply_scenario(index) end
    imgui.PopID()
    imgui.SameLine()
    if SFN1H.active == index then
        local status, detail = evaluate(s)
        colored_text(result_color(status), string.format("%s | %s | %s", status, expected_text(s), detail))
    else
        colored_text(COL_DIM, expected_text(s))
    end
end

local function draw_scenarios()
    colored_text(COL_HEAD, "SCENARIOS")
    for index, s in ipairs(SCENARIOS) do
        draw_scenario_row(index, s)
    end
    if imgui.Button("LIVE (overrides off)") then go_live() end
    imgui.SameLine()
    local enabled = geti("sfn1/test_override_enable")
    if enabled == 1 then
        colored_text(COL_WAIT, "overrides ACTIVE (plugin ignores real sim state)")
    else
        colored_text(COL_DIM, "overrides off; plugin follows the sim")
    end
end

local function draw_sfn1_outputs()
    colored_text(COL_HEAD, "SFN1 OUTPUTS")
    if not SFN1H.refs_ok then
        colored_text(COL_DIM, "(unavailable until the plugin registers its datarefs)")
        return
    end
    local state = geti("sfn1/display_state")
    local mode = geti("sfn1/mode")
    imgui.TextUnformatted(string.format("target_n1: %.1f   display_state: %d (%s)   mode: %d (%s)",
        getf("sfn1/target_n1"), state, STATE_NAMES[state] or "?", mode, MODE_NAMES[mode] or "?"))
    imgui.TextUnformatted(string.format("rat_c: %.1f   selected_temp_c: %.1f",
        getf("sfn1/rat_c"), getf("sfn1/selected_temp_c")))
    imgui.TextUnformatted(string.format("overrides: rat=%.1f pa=%.0f gnd=%d ai=%d pwr=%d",
        getf("sfn1/test_override_rat_c"), getf("sfn1/test_override_pa_ft"),
        geti("sfn1/test_override_on_ground"), geti("sfn1/test_override_anti_ice"),
        geti("sfn1/test_override_powered")))
end

local function draw_raw_inputs()
    colored_text(COL_HEAD, "RAW SIM INPUTS (for eyeball comparison)")
    for _, input in ipairs(RAW_INPUTS) do
        local text = raw_value_text(input, input.dr)
        if input.dr2 then text = text .. " / " .. raw_value_text(input, input.dr2) end
        imgui.TextUnformatted(string.format("%-22s %s", input.label, text))
    end
end

function sfn1h_build()
    draw_header()
    imgui.Separator()
    draw_scenarios()
    imgui.Separator()
    draw_sfn1_outputs()
    imgui.Separator()
    draw_raw_inputs()
end

function sfn1h_on_close()
    SFN1H.wnd = nil
end

function sfn1h_show()
    if SFN1H.wnd then return end
    SFN1H.wnd = float_wnd_create(660, 640, 1, true)
    float_wnd_set_title(SFN1H.wnd, "SFN1 Test Harness")
    float_wnd_set_imgui_builder(SFN1H.wnd, "sfn1h_build")
    float_wnd_set_onclose(SFN1H.wnd, "sfn1h_on_close")
end

function sfn1h_hide()
    if not SFN1H.wnd then return end
    local wnd = SFN1H.wnd
    SFN1H.wnd = nil
    float_wnd_destroy(wnd)
end

--------------------------------------------------------------------------------
-- Wiring
--------------------------------------------------------------------------------

resolve_sfn1_refs()
load_missing_tables()
SFN1H.tables_last_try = now()
sfn1h_show()
add_macro("SFN1 test harness: window", "sfn1h_show()", "sfn1h_hide()", "activate")
do_every_frame("sfn1h_frame()")
do_often("sfn1h_often()")
logMsg("SFN1 harness: loaded (" .. loaded_table_count() .. "/" .. #TABLE_NAMES .. " N1 tables)")
