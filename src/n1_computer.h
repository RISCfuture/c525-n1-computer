#pragma once

#include <optional>
#include <string>

#include "n1_tables.h"

namespace sfn1 {

enum class Mode { Clb, ToGa, Cru };  // knob detents, CLB left / TO-GA center / CRU right
// Ordinals are published as sfn1/display_state (docs/CONTRACTS.md); append only.
enum class DisplayState { Off, SelfTest888, Dashes, Rat, TempSet, N1, Fail };

struct InputSnapshot {
    bool powered = false;   // avionics bus energized
    bool onGround = false;  // WOW
    double ratC = 0.0;      // ram air temp °C
    double pressureAltFt = 0.0;
    int antiIce = 0;  // 0 = off, 1 = partial, 2 = all bleed anti-ice on
    bool knobPressed = false;
    bool airDataFailed = false;  // air data source feeding RAT/pressure altitude is invalid
};

struct Output {
    DisplayState state;
    double value;  // N1 % / RAT °C / selected temp °C, per state; 0 otherwise
    Mode mode;
};

/// The Safe Flight N1 computer state machine: pure logic, host-testable,
/// driven once per frame with an InputSnapshot.
class N1Computer {
public:
    /// Loads the eight schedule CSVs (n1_takeoff[_ai], n1_goaround[_ai],
    /// n1_climb[_ai], n1_cruise[_ai]) from dir. Returns false if any file is
    /// missing or malformed; missing schedules then display as dashes.
    bool loadTables(const std::string& dir);

    /// Moves the mode knob to a detent.
    void setMode(Mode mode);

    /// Adjusts the pilot-selected (assumed) takeoff temperature by deltaC
    /// (press + rotate). Ground only; starts from the last sensed RAT.
    void bumpTemp(double deltaC);

    /// Advances the device by dtSeconds and returns what the display shows.
    Output tick(const InputSnapshot& input, double dtSeconds);

    /// Clears a latched unsatisfactory power-up self-test without a power
    /// cycle, so the device recovers alongside the simulator's global
    /// "fix all systems".
    void clearLatchedFailure() { selfTestFailed_ = false; }

    /// Pilot-selected takeoff temperature, if one is dialed in.
    std::optional<double> selectedTempC() const { return selectedTempC_; }

private:
    /// The takeoff conditions the display holds after liftoff: the temperature
    /// and field elevation the ground display was reading from. Anti-ice is
    /// deliberately absent, since the AFM keeps it live.
    struct TakeoffHold {
        double oatC;
        double fieldElevationFt;
    };

    struct ScheduleTables {
        std::optional<N1Table> takeoff, takeoffAi;
        std::optional<N1Table> goAround, goAroundAi;
        std::optional<N1Table> climb, climbAi;
        std::optional<N1Table> cruise, cruiseAi;
    };

    void updatePower(const InputSnapshot& input);
    void reset();
    void handleGroundAirTransition(const InputSnapshot& input, double dtSeconds);
    void runLandingRevert(const InputSnapshot& input, double dtSeconds);
    void handleKnobPressEdge(const InputSnapshot& input);
    void armTakeoffHold();
    void releaseTakeoffHold();
    void wakeFromStandby();
    void rememberInputs(const InputSnapshot& input);

    Output display(const InputSnapshot& input, double dtSeconds);
    Output knobDisplay(const InputSnapshot& input) const;
    Output targetDisplay(const InputSnapshot& input) const;
    Output groundTargetDisplay(const InputSnapshot& input, bool antiIceOn) const;
    Output airborneTargetDisplay(const InputSnapshot& input, bool antiIceOn) const;
    Output takeoffDisplay(double oatC, double paFt, bool antiIceOn) const;
    const std::optional<N1Table>& airborneSchedule(bool antiIceOn) const;
    Output n1From(const std::optional<N1Table>& table, double oatC, double paFt) const;
    Output dashes() const;
    Output blank() const;
    Output output(DisplayState state, double value) const;
    bool selfTestRunning() const { return selfTestRemainingS_ > 0.0; }

    ScheduleTables tables_;
    Mode mode_ = Mode::ToGa;
    bool powered_ = false;
    double selfTestRemainingS_ = 0.0;
    bool standby888_ = false;
    bool landingRevertArmed_ = false;
    double sinceTouchdownS_ = 0.0;
    double airborneS_ = 0.0;
    std::optional<double> selectedTempC_;
    std::optional<TakeoffHold> takeoffHold_;
    bool rotatedThisPress_ = false;
    double lastRatC_ = 15.0;
    double lastPressureAltFt_ = 0.0;
    bool lastOnGround_ = true;
    bool lastKnobPressed_ = false;
    bool selfTestFailed_ = false;
};

}  // namespace sfn1
