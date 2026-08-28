#pragma once

#include <vector>

#include "n1_computer.h"

#include "XPLMDataAccess.h"
#include "aircraft_gate.h"

namespace sfn1 {

/// Values backing the sfn1/test_override_* datarefs; when enable is nonzero
/// the snapshot is built entirely from these instead of the simulator.
struct TestOverrides {
    int enable = 0;
    float ratC = 0.0f;
    float paFt = 0.0f;
    int onGround = 0;
    int antiIce = 0;
    int powered = 0;
    int airDataFailed = 0;
};

/// Samples the stock and cj_systems datarefs feeding the device each flight loop.
class SimInputs {
public:
    SimInputs();

    /// Builds the device input for this frame, honoring active test overrides.
    /// A pulled circuit breaker opens the device's power feed.
    InputSnapshot snapshot(const AircraftGate& gate, const TestOverrides& overrides,
                           bool knobPressed, bool breakerPulled) const;

private:
    InputSnapshot overriddenSnapshot(const TestOverrides& overrides, bool knobPressed) const;
    InputSnapshot liveSnapshot(const AircraftGate& gate, bool knobPressed) const;
    bool powered(const AircraftGate& gate) const;
    int antiIce(const AircraftGate& gate) const;
    bool airDataFailed() const;

    XPLMDataRef ratC_ = nullptr;
    XPLMDataRef pressureAltFt_ = nullptr;
    XPLMDataRef onGroundAny_ = nullptr;
    XPLMDataRef avionicsPowerOn_ = nullptr;

    /// Stock failures that invalidate the air data feeding RAT and pressure
    /// altitude. Standby sources are excluded: they drive the standby
    /// instruments, not the air data bus the N1 computer reads.
    std::vector<XPLMDataRef> airDataFailureRefs_;
};

}  // namespace sfn1
