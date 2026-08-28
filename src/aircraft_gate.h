#pragma once

#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"

namespace sfn1 {

/// Dataref handles resolved from the TorqueSim cj_systems plugin. All null
/// until AircraftGate resolves them; null again after an aircraft change.
struct AfmHandles {
    XPLMDataRef engN1Left = nullptr;
    XPLMDataRef engN1Right = nullptr;
    XPLMDataRef iceBleed = nullptr;
    XPLMDataRef iceWingLeft = nullptr;
    XPLMDataRef iceWingRight = nullptr;
    XPLMDataRef avionicsBusLeftVolts = nullptr;
    XPLMDataRef avionicsBusRightVolts = nullptr;
};

/// Activates the plugin only while the TorqueSim CJ525 is the user aircraft.
///
/// On XPLM_MSG_PLANE_LOADED the gate matches the user aircraft file against
/// CJ525.acf, then resolves the afm/cj datarefs from a deferred flight loop,
/// since cj_systems registers them only after the aircraft's plugins start.
/// Resolution is all-or-nothing and retries for as long as the CJ525 remains
/// loaded: a partially resolved gate would silently read anti-ice as off and
/// publish the wrong schedule, which is worse than staying inactive.
class AircraftGate {
public:
    ~AircraftGate();

    /// Handles a user-aircraft XPLM_MSG_PLANE_LOADED message.
    void onPlaneLoaded();

    /// Cancels any pending resolve and deactivates; call from XPluginDisable.
    void shutdown();

    /// True once the CJ525 is loaded and its datarefs are resolved.
    bool isActive() const { return active_; }

    /// The resolved cj_systems dataref handles; valid only while isActive().
    const AfmHandles& handles() const { return handles_; }

private:
    static float retryCallback(float, float, int, void* refcon);

    bool userAircraftIsCJ525() const;
    void beginDeferredResolve();
    void cancelDeferredResolve();
    void deactivate();
    float continueResolving();
    bool resolveHandles();

    AfmHandles handles_;
    bool active_ = false;
    XPLMFlightLoopID retryLoop_ = nullptr;
    float retryElapsedSeconds_ = 0.0f;
    const char* missingDataRef_ = nullptr;
    bool warnedSlowResolve_ = false;
};

}  // namespace sfn1
