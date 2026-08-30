#include "aircraft_gate.h"

#include <cstring>

#include "XPLMPlanes.h"
#include "XPLMUtilities.h"

namespace sfn1 {
namespace {

constexpr float kRetryIntervalSeconds = 0.5f;
constexpr float kSlowResolveWarningSeconds = 30.0f;
constexpr const char* kAircraftFile = "CJ525.acf";

void log(const char* message) {
    XPLMDebugString("SafeFlightN1: ");
    XPLMDebugString(message);
    XPLMDebugString("\n");
}

}  // namespace

AircraftGate::~AircraftGate() { shutdown(); }

void AircraftGate::onPlaneLoaded() {
    deactivate();
    cancelDeferredResolve();
    if (userAircraftIsCJ525()) {
        log("CJ525 detected; resolving afm/cj datarefs");
        beginDeferredResolve();
    }
}

void AircraftGate::shutdown() {
    cancelDeferredResolve();
    deactivate();
}

bool AircraftGate::userAircraftIsCJ525() const {
    char file[256] = {};
    char path[512] = {};
    XPLMGetNthAircraftModel(XPLM_USER_AIRCRAFT, file, path);
    return std::strcmp(file, kAircraftFile) == 0;
}

void AircraftGate::beginDeferredResolve() {
    retryElapsedSeconds_ = 0.0f;
    warnedSlowResolve_ = false;
    XPLMCreateFlightLoop_t params = {};
    params.structSize = sizeof(params);
    params.phase = xplm_FlightLoop_Phase_AfterFlightModel;
    params.callbackFunc = retryCallback;
    params.refcon = this;
    retryLoop_ = XPLMCreateFlightLoop(&params);
    XPLMScheduleFlightLoop(retryLoop_, kRetryIntervalSeconds, 1);
}

void AircraftGate::cancelDeferredResolve() {
    if (!retryLoop_) return;
    XPLMDestroyFlightLoop(retryLoop_);
    retryLoop_ = nullptr;
}

void AircraftGate::deactivate() {
    active_ = false;
    handles_ = {};
}

float AircraftGate::retryCallback(float, float, int, void* refcon) {
    return static_cast<AircraftGate*>(refcon)->continueResolving();
}

float AircraftGate::continueResolving() {
    if (resolveHandles()) {
        active_ = true;
        log("afm/cj datarefs resolved; gate active");
        return 0.0f;
    }
    retryElapsedSeconds_ += kRetryIntervalSeconds;
    if (!warnedSlowResolve_ && retryElapsedSeconds_ >= kSlowResolveWarningSeconds) {
        warnedSlowResolve_ = true;
        log("still waiting on cj_systems; unresolved dataref:");
        log(missingDataRef_ ? missingDataRef_ : "(unknown)");
    }
    return kRetryIntervalSeconds;
}

/// All-or-nothing: a half-resolved gate reports anti-ice off and picks the
/// dry schedule, which reads as a healthy display showing the wrong target.
bool AircraftGate::resolveHandles() {
    struct Binding {
        const char* name;
        XPLMDataRef AfmHandles::* field;
    };
    static constexpr Binding kBindings[] = {
        {"afm/cj/f/gauges/eng_n1_l", &AfmHandles::engN1Left},
        {"afm/cj/f/gauges/eng_n1_r", &AfmHandles::engN1Right},
        {"afm/cj/switch_panel/ice_bleed", &AfmHandles::iceWindshieldBleed},
        {"afm/cj/switch_panel/ice_wing_l", &AfmHandles::iceWingLeft},
        {"afm/cj/switch_panel/ice_wing_r", &AfmHandles::iceWingRight},
        {"afm/cj/f/elec/V_bus_av_l_x_over", &AfmHandles::avionicsBusLeftVolts},
        {"afm/cj/f/elec/V_bus_av_r_feed_ext", &AfmHandles::avionicsBusRightVolts},
    };

    AfmHandles resolved;
    for (const Binding& binding : kBindings) {
        resolved.*(binding.field) = XPLMFindDataRef(binding.name);
        if (!(resolved.*(binding.field))) {
            missingDataRef_ = binding.name;
            return false;
        }
    }
    missingDataRef_ = nullptr;
    handles_ = resolved;
    return true;
}

}  // namespace sfn1
