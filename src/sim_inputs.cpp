#include "sim_inputs.h"

#include <algorithm>

namespace sfn1 {
namespace {

constexpr float kBusEnergizedVolts = 18.0f;

// X-Plane failure_enum, per the sim's own flight-spec parser: 0 always_work,
// 1 fail_mean_time_in_hours, 2 fail_exact_time_in_hours, 3 fail_at_speed_in_knots,
// 4 fail_at_altitude_in_feet, 5 fail_at_command_trigger, 6 inoperative.
// Values 1-5 only *arm* a failure - the system is still working - so the test
// is equality with 6, never "nonzero".
constexpr int kInoperative = 6;

// Air data sources feeding the N1 computer. Pitot is absent by design: the
// device uses RAT and pressure altitude, never airspeed.
constexpr const char* kAirDataFailureDataRefs[] = {
    "sim/operation/failures/rel_adc_comp",    // air data computer 1 (pilot)
    "sim/operation/failures/rel_adc_comp_2",  // air data computer 2
    "sim/operation/failures/rel_static",      // static 1 - blockage
    "sim/operation/failures/rel_static2",     // static 2 - blockage
    "sim/operation/failures/rel_tat_2",       // TAT probe (no pilot-side dataref exists)
};

// The three anti-ice switches are three-position: the cockpit object animates
// each of ice_bleed, ice_wing_l and ice_wing_r over rotate keys -1 / 0 / +1
// with the centre at rest, and TorqueSim puts OFF at 0 on every such switch
// (batt is EMER -1 / OFF 0 / BATT +1). Both non-zero detents are therefore
// selected positions - W/S BLEED AIR is HI and LO either side of OFF - so the
// test is "not centred", never "positive", which would read LO as off and
// dash on a fully configured airplane.
bool switchIsOn(XPLMDataRef ref) { return ref && XPLMGetDatai(ref) != 0; }

bool busIsEnergized(XPLMDataRef ref) { return ref && XPLMGetDataf(ref) >= kBusEnergizedVolts; }

}  // namespace

SimInputs::SimInputs()
    : ratC_(XPLMFindDataRef("sim/cockpit2/temperature/outside_air_LE_temp_degc")),
      pressureAltFt_(XPLMFindDataRef("sim/flightmodel2/position/pressure_altitude")),
      onGroundAny_(XPLMFindDataRef("sim/flightmodel/failures/onground_any")),
      avionicsPowerOn_(XPLMFindDataRef("sim/cockpit2/switches/avionics_power_on")) {
    for (const char* name : kAirDataFailureDataRefs) {
        if (XPLMDataRef ref = XPLMFindDataRef(name)) airDataFailureRefs_.push_back(ref);
    }
}

InputSnapshot SimInputs::snapshot(const AircraftGate& gate, const TestOverrides& overrides,
                                  bool knobPressed, bool breakerPulled) const {
    InputSnapshot in = overrides.enable ? overriddenSnapshot(overrides, knobPressed)
                                        : liveSnapshot(gate, knobPressed);
    // The device cannot tell an open breaker from a dead bus, so a pulled
    // breaker is simply an unpowered device - including under test overrides,
    // which lets the scenario harness exercise it.
    if (breakerPulled) in.powered = false;
    return in;
}

InputSnapshot SimInputs::overriddenSnapshot(const TestOverrides& overrides,
                                            bool knobPressed) const {
    InputSnapshot in{};
    in.powered = overrides.powered != 0;
    in.onGround = overrides.onGround != 0;
    in.ratC = overrides.ratC;
    in.pressureAltFt = overrides.paFt;
    in.antiIce = overrides.antiIce;
    in.knobPressed = knobPressed;
    in.airDataFailed = overrides.airDataFailed != 0;
    return in;
}

InputSnapshot SimInputs::liveSnapshot(const AircraftGate& gate, bool knobPressed) const {
    InputSnapshot in{};
    in.powered = powered(gate);
    in.onGround = onGroundAny_ && XPLMGetDatai(onGroundAny_) != 0;
    in.ratC = ratC_ ? XPLMGetDataf(ratC_) : 0.0;
    in.pressureAltFt = pressureAltFt_ ? XPLMGetDataf(pressureAltFt_) : 0.0;
    in.antiIce = antiIce(gate);
    in.knobPressed = knobPressed;
    in.airDataFailed = airDataFailed();
    return in;
}

bool SimInputs::powered(const AircraftGate& gate) const {
    const bool switchOn = avionicsPowerOn_ && XPLMGetDatai(avionicsPowerOn_) != 0;
    if (!gate.isActive()) return switchOn;
    const AfmHandles& afm = gate.handles();
    if (!afm.avionicsBusLeftVolts && !afm.avionicsBusRightVolts) return switchOn;
    return switchOn && (busIsEnergized(afm.avionicsBusLeftVolts) ||
                        busIsEnergized(afm.avionicsBusRightVolts));
}

bool SimInputs::airDataFailed() const {
    return std::any_of(airDataFailureRefs_.begin(), airDataFailureRefs_.end(),
                       [](XPLMDataRef ref) { return XPLMGetDatai(ref) == kInoperative; });
}

/// "All bleed air anti-ice (W/S, ENG, WING) must be selected on for anti-ice
/// power setting. If anti-ice is partially activated, '---' will be displayed"
/// (AFM Supplement 6, p. S6-5). The airplane spreads those three over three
/// switches: W/S BLEED AIR, and the two WING/ENGINE switches.
int SimInputs::antiIce(const AircraftGate& gate) const {
    if (!gate.isActive()) return 0;
    const AfmHandles& afm = gate.handles();
    const int engaged = (switchIsOn(afm.iceWindshieldBleed) ? 1 : 0) +
                        (switchIsOn(afm.iceWingLeft) ? 1 : 0) +
                        (switchIsOn(afm.iceWingRight) ? 1 : 0);
    if (engaged == 0) return 0;
    if (engaged == 3) return 2;
    return 1;
}

}  // namespace sfn1
