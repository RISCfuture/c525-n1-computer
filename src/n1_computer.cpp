#include "n1_computer.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace sfn1 {
namespace {

constexpr double kSelfTestSeconds = 5.0;
constexpr double kLandingRevertSeconds = 60.0;
// A landing only counts after real flight; placing the aircraft in the sim can
// blip the weight-on-wheels signal as it settles onto the gear.
constexpr double kMinAirborneForLandingS = 10.0;
constexpr double kGoAroundCeilingFt = 15500.0;
// The readout is three digits plus a sign, so temperatures outside this range
// cannot be shown; the real RAT indication saturates rather than running away.
constexpr double kSelectedTempLimitC = 99.0;

std::optional<N1Table> loadCsvFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) return std::nullopt;
    std::ostringstream text;
    text << file.rdbuf();
    return N1Table::loadFromCsv(text.str());
}

}  // namespace

bool N1Computer::loadTables(const std::string& dir) {
    tables_.takeoff = loadCsvFile(dir + "/n1_takeoff.csv");
    tables_.takeoffAi = loadCsvFile(dir + "/n1_takeoff_ai.csv");
    tables_.goAround = loadCsvFile(dir + "/n1_goaround.csv");
    tables_.goAroundAi = loadCsvFile(dir + "/n1_goaround_ai.csv");
    tables_.climb = loadCsvFile(dir + "/n1_climb.csv");
    tables_.climbAi = loadCsvFile(dir + "/n1_climb_ai.csv");
    tables_.cruise = loadCsvFile(dir + "/n1_cruise.csv");
    tables_.cruiseAi = loadCsvFile(dir + "/n1_cruise_ai.csv");
    return tables_.takeoff && tables_.takeoffAi && tables_.goAround && tables_.goAroundAi &&
           tables_.climb && tables_.climbAi && tables_.cruise && tables_.cruiseAi;
}

void N1Computer::setMode(Mode mode) {
    if (mode == mode_) return;
    mode_ = mode;
    releaseTakeoffHold();
    wakeFromStandby();
}

void N1Computer::bumpTemp(double deltaC) {
    if (!powered_ || !lastOnGround_) return;
    wakeFromStandby();
    selectedTempC_ = std::clamp(selectedTempC_.value_or(lastRatC_) + deltaC,
                                -kSelectedTempLimitC, kSelectedTempLimitC);
    rotatedThisPress_ = true;
}

Output N1Computer::tick(const InputSnapshot& input, double dtSeconds) {
    updatePower(input);
    if (!powered_) return output(DisplayState::Off, 0.0);
    handleGroundAirTransition(input, dtSeconds);
    runLandingRevert(input, dtSeconds);
    handleKnobPressEdge(input);
    const Output shown = display(input, dtSeconds);
    rememberInputs(input);
    return shown;
}

void N1Computer::updatePower(const InputSnapshot& input) {
    if (!input.powered) {
        if (powered_) reset();
        return;
    }
    if (!powered_) {
        powered_ = true;
        selfTestRemainingS_ = kSelfTestSeconds;
        selfTestFailed_ = input.airDataFailed;
    }
}

void N1Computer::reset() {
    powered_ = false;
    selfTestRemainingS_ = 0.0;
    selfTestFailed_ = false;
    standby888_ = false;
    landingRevertArmed_ = false;
    sinceTouchdownS_ = 0.0;
    airborneS_ = 0.0;
    selectedTempC_.reset();
    takeoffHold_.reset();
    rotatedThisPress_ = false;
    lastKnobPressed_ = false;
}

void N1Computer::handleGroundAirTransition(const InputSnapshot& input, double dtSeconds) {
    const bool liftedOff = lastOnGround_ && !input.onGround;
    const bool touchedDown = !lastOnGround_ && input.onGround;
    if (!input.onGround) airborneS_ += dtSeconds;
    if (liftedOff) {
        armTakeoffHold();
        wakeFromStandby();
    }
    if (touchedDown) {
        releaseTakeoffHold();
        if (airborneS_ >= kMinAirborneForLandingS) {
            landingRevertArmed_ = true;
            sinceTouchdownS_ = 0.0;
        }
        airborneS_ = 0.0;
    }
}

void N1Computer::runLandingRevert(const InputSnapshot& input, double dtSeconds) {
    if (!landingRevertArmed_ || !input.onGround) return;
    sinceTouchdownS_ += dtSeconds;
    if (sinceTouchdownS_ >= kLandingRevertSeconds) {
        standby888_ = true;
        landingRevertArmed_ = false;
    }
}

void N1Computer::handleKnobPressEdge(const InputSnapshot& input) {
    if (!input.knobPressed || lastKnobPressed_) return;
    rotatedThisPress_ = false;
    wakeFromStandby();
}

/// "After the airplane is inflight, the display will continue to indicate
/// takeoff percent N1 based on the selected temperature, field elevation and
/// anti-ice until another mode is selected" (AFM Supplement 6, p. S6-6). Only
/// TO/GA was showing a takeoff target on the ground, so only TO/GA has one to
/// hold; lifting off in CLB or CRU goes straight to that mode's schedule.
void N1Computer::armTakeoffHold() {
    if (mode_ != Mode::ToGa) return;
    takeoffHold_ = TakeoffHold{selectedTempC_.value_or(lastRatC_), lastPressureAltFt_};
}

/// Ends the hold, and with it the selected temperature it captured: from here
/// the display works from "RAT and current pressure altitude ... for that
/// mode". Touchdown ends it too - the ground rules are separately specified,
/// and the takeoff the selection belonged to is over.
void N1Computer::releaseTakeoffHold() {
    if (!takeoffHold_) return;
    takeoffHold_.reset();
    selectedTempC_.reset();
}

void N1Computer::wakeFromStandby() {
    standby888_ = false;
    landingRevertArmed_ = false;
    sinceTouchdownS_ = 0.0;
}

void N1Computer::rememberInputs(const InputSnapshot& input) {
    lastRatC_ = input.ratC;
    lastPressureAltFt_ = input.pressureAltFt;
    lastOnGround_ = input.onGround;
    lastKnobPressed_ = input.knobPressed;
}

Output N1Computer::display(const InputSnapshot& input, double dtSeconds) {
    if (selfTestFailed_) return blank();
    if (selfTestRunning()) {
        selfTestRemainingS_ -= dtSeconds;
        return output(DisplayState::SelfTest888, 0.0);
    }
    if (input.airDataFailed) return blank();
    if (standby888_) return output(DisplayState::SelfTest888, 0.0);
    if (input.knobPressed) return knobDisplay(input);
    return targetDisplay(input);
}

Output N1Computer::knobDisplay(const InputSnapshot& input) const {
    if (rotatedThisPress_ && selectedTempC_)
        return output(DisplayState::TempSet, *selectedTempC_);
    return output(DisplayState::Rat, input.ratC);
}

Output N1Computer::targetDisplay(const InputSnapshot& input) const {
    if (input.antiIce == 1) return dashes();
    const bool antiIceOn = input.antiIce == 2;
    if (input.onGround) return groundTargetDisplay(input, antiIceOn);
    return airborneTargetDisplay(input, antiIceOn);
}

Output N1Computer::groundTargetDisplay(const InputSnapshot& input, bool antiIceOn) const {
    if (mode_ != Mode::ToGa) return dashes();
    return takeoffDisplay(selectedTempC_.value_or(input.ratC), input.pressureAltFt, antiIceOn);
}

Output N1Computer::airborneTargetDisplay(const InputSnapshot& input, bool antiIceOn) const {
    if (takeoffHold_)
        return takeoffDisplay(takeoffHold_->oatC, takeoffHold_->fieldElevationFt, antiIceOn);
    if (mode_ == Mode::ToGa && input.pressureAltFt > kGoAroundCeilingFt) return dashes();
    return n1From(airborneSchedule(antiIceOn), input.ratC, input.pressureAltFt);
}

Output N1Computer::takeoffDisplay(double oatC, double paFt, bool antiIceOn) const {
    return n1From(antiIceOn ? tables_.takeoffAi : tables_.takeoff, oatC, paFt);
}

const std::optional<N1Table>& N1Computer::airborneSchedule(bool antiIceOn) const {
    switch (mode_) {
        case Mode::Clb: return antiIceOn ? tables_.climbAi : tables_.climb;
        case Mode::Cru: return antiIceOn ? tables_.cruiseAi : tables_.cruise;
        case Mode::ToGa: break;
    }
    return antiIceOn ? tables_.goAroundAi : tables_.goAround;
}

Output N1Computer::n1From(const std::optional<N1Table>& table, double oatC,
                          double paFt) const {
    if (!table) return dashes();
    const auto n1 = table->lookup(oatC, paFt);
    if (!n1) return dashes();
    return output(DisplayState::N1, *n1);
}

Output N1Computer::dashes() const { return output(DisplayState::Dashes, 0.0); }

/// "The display will blank for any failure" (AFM Supplement 6, p. S6-6). An
/// unsatisfactory power-up self-test latches until the next power cycle; a
/// failure arising later clears as soon as the air data source is valid again.
Output N1Computer::blank() const { return output(DisplayState::Fail, 0.0); }

Output N1Computer::output(DisplayState state, double value) const {
    return {state, value, mode_};
}

}  // namespace sfn1
