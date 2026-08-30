#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "XPLMDataAccess.h"
#include "XPLMDefs.h"
#include "XPLMMenus.h"
#include "XPLMPlanes.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#include "aircraft_gate.h"
#include "ini.h"
#include "plugin_paths.h"
#include "sim_inputs.h"
#include "window.h"

#ifndef SFN1_VERSION
#define SFN1_VERSION "0.0.0-dev"
#endif

namespace {

using namespace sfn1;

struct PublishedState {
    float targetN1 = -1.0f;
    int mode = 1;
    int displayState = 0;
    float selectedTempC = 0.0f;
    float ratC = 0.0f;
    int airDataFailed = 0;
};

AircraftGate gGate;
std::unique_ptr<SimInputs> gInputs;
N1Computer gDevice;
std::unique_ptr<N1SettingWindow> gWindow;
TestOverrides gOverrides;
PublishedState gPublished;
bool gKnobHeld = false;
XPLMCommandRef gCmdFixAllSystems = nullptr;
int gModeIndex = 1;

/// The "N1 IND" breaker in the AVIONICS DC section of the right circuit-breaker
/// panel, fed from the right feed extension bus (FlightSafety CJ1 PTM Vol 2,
/// Figures 2-7 and 2-8). Pulling it power-cycles this box alone, rather than
/// dropping every avionics box with the master switch.
int gBreakerPulled = 0;

/// The Operating Manual's hot-and-high climb/cruise trim, off unless
/// config.ini turns it on. Writable as sfn1/isa_trim so both behaviours can be
/// compared in one session without a restart. Kept out of settings.ini, which
/// the window rewrites wholesale whenever it closes.
int gIsaTrim = 0;

void applyConfig() {
    const auto config = readIni(pluginDir() + "/config.ini");
    gIsaTrim = iniValue(config, "isa_trim", 0) != 0 ? 1 : 0;
    gDevice.setIsaTrim(gIsaTrim != 0);
}

XPLMFlightLoopID gMainLoop = nullptr;
XPLMMenuID gMenu = nullptr;
XPLMCommandRef gCmdToggleWindow = nullptr;
XPLMCommandRef gCmdModeUp = nullptr;
XPLMCommandRef gCmdModeDown = nullptr;
XPLMCommandRef gCmdKnobPress = nullptr;
XPLMCommandRef gCmdTempUp = nullptr;
XPLMCommandRef gCmdTempDown = nullptr;
XPLMCommandRef gCmdBreakerPull = nullptr;
XPLMCommandRef gCmdBreakerReset = nullptr;
XPLMCommandRef gCmdBreakerToggle = nullptr;
int gBreakerMenuItem = -1;
std::vector<XPLMDataRef> gAccessors;

void loadDeviceTables() {
    if (!gDevice.loadTables(pluginDir() + "/data"))
        XPLMDebugString(
            "SafeFlightN1: N1 schedule tables missing or malformed; "
            "affected modes will show dashes\n");
}

int readInt(void* refcon) { return *static_cast<int*>(refcon); }
void writeInt(void* refcon, int value) { *static_cast<int*>(refcon) = value; }
float readFloat(void* refcon) { return *static_cast<float*>(refcon); }
void writeFloat(void* refcon, float value) { *static_cast<float*>(refcon) = value; }

void registerIntDataRef(const char* name, int* storage, bool writable) {
    gAccessors.push_back(XPLMRegisterDataAccessor(
        name, xplmType_Int, writable ? 1 : 0, readInt, writable ? writeInt : nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        storage, writable ? storage : nullptr));
}

void registerFloatDataRef(const char* name, float* storage, bool writable) {
    gAccessors.push_back(XPLMRegisterDataAccessor(
        name, xplmType_Float, writable ? 1 : 0, nullptr, nullptr, readFloat,
        writable ? writeFloat : nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, storage, writable ? storage : nullptr));
}

void registerDataRefs() {
    registerFloatDataRef("sfn1/target_n1", &gPublished.targetN1, false);
    registerIntDataRef("sfn1/mode", &gPublished.mode, false);
    registerIntDataRef("sfn1/display_state", &gPublished.displayState, false);
    registerFloatDataRef("sfn1/selected_temp_c", &gPublished.selectedTempC, false);
    registerFloatDataRef("sfn1/rat_c", &gPublished.ratC, false);
    registerIntDataRef("sfn1/air_data_failed", &gPublished.airDataFailed, false);
    registerIntDataRef("sfn1/breaker_pulled", &gBreakerPulled, true);
    registerIntDataRef("sfn1/isa_trim", &gIsaTrim, true);
    registerIntDataRef("sfn1/test_override_enable", &gOverrides.enable, true);
    registerFloatDataRef("sfn1/test_override_rat_c", &gOverrides.ratC, true);
    registerFloatDataRef("sfn1/test_override_pa_ft", &gOverrides.paFt, true);
    registerIntDataRef("sfn1/test_override_on_ground", &gOverrides.onGround, true);
    registerIntDataRef("sfn1/test_override_anti_ice", &gOverrides.antiIce, true);
    registerIntDataRef("sfn1/test_override_powered", &gOverrides.powered, true);
    registerIntDataRef("sfn1/test_override_air_data_failed", &gOverrides.airDataFailed, true);
}

void unregisterDataRefs() {
    for (XPLMDataRef accessor : gAccessors) XPLMUnregisterDataAccessor(accessor);
    gAccessors.clear();
}

Mode modeFromIndex(int index) {
    switch (index) {
        case 0: return Mode::Clb;
        case 2: return Mode::Cru;
        default: return Mode::ToGa;
    }
}

int indexFromMode(Mode mode) {
    switch (mode) {
        case Mode::Clb: return 0;
        case Mode::ToGa: return 1;
        case Mode::Cru: return 2;
    }
    return 1;
}

void bumpMode(int delta) {
    const int next = gModeIndex + delta;
    if (next < 0 || next > 2) return;
    gModeIndex = next;
    gDevice.setMode(modeFromIndex(gModeIndex));
}

/// The N1 computer is STC'd equipment on the Citation 525, so with any other
/// aircraft loaded it simply is not installed. Test overrides bypass the gate
/// so the scenario harnesses can still drive the logic without the aircraft.
bool deviceIsInstalled() { return gGate.isActive() || gOverrides.enable != 0; }

void toggleWindow() {
    if (!deviceIsInstalled()) {
        XPLMDebugString(
            "SafeFlightN1: not installed in this aircraft; "
            "window unavailable\n");
        return;
    }
    if (!gWindow)
        gWindow = std::make_unique<N1SettingWindow>(
            N1SettingWindow::Actions{[](int delta) { bumpMode(delta); },
                                     [](double deltaC) { gDevice.bumpTemp(deltaC); }});
    gWindow->toggle();
}

int handleToggleWindow(XPLMCommandRef, XPLMCommandPhase phase, void*) {
    if (phase == xplm_CommandBegin) toggleWindow();
    return 0;
}

int handleModeUp(XPLMCommandRef, XPLMCommandPhase phase, void*) {
    if (phase == xplm_CommandBegin) bumpMode(+1);
    return 0;
}

int handleModeDown(XPLMCommandRef, XPLMCommandPhase phase, void*) {
    if (phase == xplm_CommandBegin) bumpMode(-1);
    return 0;
}

int handleKnobPress(XPLMCommandRef, XPLMCommandPhase phase, void*) {
    gKnobHeld = phase != xplm_CommandEnd;
    return 0;
}

int handleTempUp(XPLMCommandRef, XPLMCommandPhase phase, void*) {
    if (phase == xplm_CommandBegin) gDevice.bumpTemp(+1.0);
    return 0;
}

int handleTempDown(XPLMCommandRef, XPLMCommandPhase phase, void*) {
    if (phase == xplm_CommandBegin) gDevice.bumpTemp(-1.0);
    return 0;
}

int handleBreakerPull(XPLMCommandRef, XPLMCommandPhase phase, void*) {
    if (phase == xplm_CommandBegin) gBreakerPulled = 1;
    return 0;
}

int handleBreakerReset(XPLMCommandRef, XPLMCommandPhase phase, void*) {
    if (phase == xplm_CommandBegin) gBreakerPulled = 0;
    return 0;
}

int handleBreakerToggle(XPLMCommandRef, XPLMCommandPhase phase, void*) {
    if (phase == xplm_CommandBegin) gBreakerPulled = gBreakerPulled ? 0 : 1;
    return 0;
}

XPLMCommandRef createCommand(const char* name, const char* description,
                             XPLMCommandCallback_f handler) {
    XPLMCommandRef command = XPLMCreateCommand(name, description);
    XPLMRegisterCommandHandler(command, handler, 0, nullptr);
    return command;
}

/// The stock "Fix All Systems" clears the sim's failures; clear the device's
/// latched self-test result with them, so the box recovers like everything else
/// instead of silently waiting for a power cycle.
int handleFixAllSystems(XPLMCommandRef, XPLMCommandPhase phase, void*) {
    if (phase == xplm_CommandBegin) gDevice.clearLatchedFailure();
    return 1;
}

void createCommands() {
    gCmdToggleWindow = createCommand(
        "sfn1/toggle_window", "Toggle the Safe Flight N1 Computer window", handleToggleWindow);
    gCmdModeUp =
        createCommand("sfn1/mode_up", "N1 computer mode knob one detent right", handleModeUp);
    gCmdModeDown = createCommand("sfn1/mode_down", "N1 computer mode knob one detent left",
                                 handleModeDown);
    gCmdKnobPress =
        createCommand("sfn1/knob_press", "Hold the N1 computer knob pressed", handleKnobPress);
    gCmdTempUp =
        createCommand("sfn1/temp_up", "N1 computer selected temperature up", handleTempUp);
    gCmdTempDown = createCommand("sfn1/temp_down", "N1 computer selected temperature down",
                                 handleTempDown);
    gCmdBreakerPull = createCommand("sfn1/breaker_pull", "Pull the N1 IND circuit breaker",
                                    handleBreakerPull);
    gCmdBreakerReset = createCommand("sfn1/breaker_reset", "Reset the N1 IND circuit breaker",
                                     handleBreakerReset);
    gCmdBreakerToggle =
        createCommand("sfn1/breaker_toggle", "Pull or reset the N1 IND circuit breaker",
                      handleBreakerToggle);
    gCmdFixAllSystems = XPLMFindCommand("sim/operation/fix_all_systems");
    if (gCmdFixAllSystems) {
        XPLMRegisterCommandHandler(gCmdFixAllSystems, handleFixAllSystems, 0, nullptr);
    }
}

void destroyCommandHandlers() {
    if (gCmdFixAllSystems) {
        XPLMUnregisterCommandHandler(gCmdFixAllSystems, handleFixAllSystems, 0, nullptr);
    }
    XPLMUnregisterCommandHandler(gCmdToggleWindow, handleToggleWindow, 0, nullptr);
    XPLMUnregisterCommandHandler(gCmdModeUp, handleModeUp, 0, nullptr);
    XPLMUnregisterCommandHandler(gCmdModeDown, handleModeDown, 0, nullptr);
    XPLMUnregisterCommandHandler(gCmdKnobPress, handleKnobPress, 0, nullptr);
    XPLMUnregisterCommandHandler(gCmdTempUp, handleTempUp, 0, nullptr);
    XPLMUnregisterCommandHandler(gCmdTempDown, handleTempDown, 0, nullptr);
    XPLMUnregisterCommandHandler(gCmdBreakerPull, handleBreakerPull, 0, nullptr);
    XPLMUnregisterCommandHandler(gCmdBreakerReset, handleBreakerReset, 0, nullptr);
    XPLMUnregisterCommandHandler(gCmdBreakerToggle, handleBreakerToggle, 0, nullptr);
}

// The breaker item names the action it performs, so its label follows the
// breaker's position.
constexpr const char* kBreakerPullLabel = "Pull N1 IND Circuit Breaker";
constexpr const char* kBreakerResetLabel = "Reset N1 IND Circuit Breaker";

void createMenu() {
    const int slot =
        XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Safe Flight N1 Computer", nullptr, 0);
    gMenu = XPLMCreateMenu("Safe Flight N1 Computer", XPLMFindPluginsMenu(), slot, nullptr,
                           nullptr);
    XPLMAppendMenuItemWithCommand(gMenu, "Toggle N1 Computer Window", gCmdToggleWindow);
    gBreakerMenuItem =
        XPLMAppendMenuItemWithCommand(gMenu, kBreakerPullLabel, gCmdBreakerToggle);
}

void destroyMenu() {
    if (!gMenu) return;
    XPLMDestroyMenu(gMenu);
    gMenu = nullptr;
}

constexpr double kNoSelectedTempC = -999.0;
constexpr float kNightDimFloor = 0.45f;

void publish(const InputSnapshot& input, const Output& output) {
    gPublished.ratC = static_cast<float>(input.ratC);
    gPublished.mode = indexFromMode(output.mode);
    gPublished.displayState = static_cast<int>(output.state);
    gPublished.targetN1 =
        output.state == DisplayState::N1 ? static_cast<float>(output.value) : -1.0f;
    gPublished.selectedTempC =
        static_cast<float>(gDevice.selectedTempC().value_or(kNoSelectedTempC));
    gPublished.airDataFailed = input.airDataFailed ? 1 : 0;
}

bool knobHeld() { return gKnobHeld || (gWindow && gWindow->knobHeldByMouse()); }

XPLMDataRef gInstrumentLightRef = nullptr;
bool gGateWasActive = false;

float segmentBrightness() {
    if (!gGate.isActive()) {
        gGateWasActive = false;
        gInstrumentLightRef = nullptr;
        return 1.0f;
    }
    if (!gGateWasActive) {
        gGateWasActive = true;
        gInstrumentLightRef = XPLMFindDataRef("afm/cj/f/lits/instruments_center");
    }
    if (!gInstrumentLightRef) return 1.0f;
    // The rheostat backlights the panel legends; the LED readout runs at full
    // daylight intensity until the panel lights come up for night operation.
    const float lit = std::clamp(XPLMGetDataf(gInstrumentLightRef), 0.0f, 1.0f);
    if (lit <= 0.01f) return 1.0f;
    return kNightDimFloor + (1.0f - kNightDimFloor) * lit;
}

/// Publishes an inert state and closes the window while the device is not
/// installed, so nothing downstream reads a Citation 525 target in another
/// airframe.
void idleUninstalled() {
    gPublished = PublishedState{};
    gPublished.mode = gModeIndex;
    gPublished.selectedTempC = static_cast<float>(kNoSelectedTempC);
    if (gWindow && gWindow->GetVisible()) gWindow->SetVisible(false);
}

void syncMenuAvailability(bool installed) {
    static int lastState = -1;
    const int state = installed ? 1 : 0;
    if (!gMenu || state == lastState) return;
    lastState = state;
    XPLMEnableMenuItem(gMenu, 0, state);
    if (gBreakerMenuItem >= 0) XPLMEnableMenuItem(gMenu, gBreakerMenuItem, state);
}

/// Tracks the breaker itself rather than the command that last moved it, so
/// writing sfn1/breaker_pulled directly relabels the menu as well.
void syncBreakerMenuItem() {
    static int lastPulled = -1;
    const int pulled = gBreakerPulled ? 1 : 0;
    if (!gMenu || gBreakerMenuItem < 0 || pulled == lastPulled) return;
    lastPulled = pulled;
    XPLMSetMenuItemName(gMenu, gBreakerMenuItem,
                        pulled ? kBreakerResetLabel : kBreakerPullLabel, 0);
}

float onFlightLoop(float elapsedSinceLastCall, float, int, void*) {
    const bool installed = deviceIsInstalled();
    syncMenuAvailability(installed);
    syncBreakerMenuItem();
    if (!installed) {
        idleUninstalled();
        return -1.0f;
    }
    gDevice.setIsaTrim(gIsaTrim != 0);  // sfn1/isa_trim is writable, so re-read it each frame
    const InputSnapshot input =
        gInputs->snapshot(gGate, gOverrides, knobHeld(), gBreakerPulled != 0);
    const Output output = gDevice.tick(input, elapsedSinceLastCall);
    publish(input, output);
    if (gWindow) gWindow->showValues(input, output, segmentBrightness());
    return -1.0f;
}

void createMainLoop() {
    XPLMCreateFlightLoop_t params = {};
    params.structSize = sizeof(params);
    params.phase = xplm_FlightLoop_Phase_AfterFlightModel;
    params.callbackFunc = onFlightLoop;
    params.refcon = nullptr;
    gMainLoop = XPLMCreateFlightLoop(&params);
}

void destroyMainLoop() {
    if (!gMainLoop) return;
    XPLMDestroyFlightLoop(gMainLoop);
    gMainLoop = nullptr;
}

}  // namespace

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    std::snprintf(outName, 256, "Safe Flight N1 Computer");
    std::snprintf(outSig, 256, "com.timothymorgan.safeflightn1");
    std::snprintf(outDesc, 256,
                  "Safe Flight N1 Computer (C-12732-1) for the TorqueSim CJ525 (v%s)",
                  SFN1_VERSION);
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    gInputs = std::make_unique<SimInputs>();
    ensureSharedFontAtlas();
    loadDeviceTables();
    applyConfig();
    createCommands();
    registerDataRefs();
    createMenu();
    createMainLoop();
    return 1;
}

PLUGIN_API void XPluginStop(void) {
    gWindow.reset();
    releaseSharedFontAtlas();
    destroyMainLoop();
    destroyMenu();
    unregisterDataRefs();
    destroyCommandHandlers();
    gInputs.reset();
}

PLUGIN_API int XPluginEnable(void) {
    XPLMScheduleFlightLoop(gMainLoop, -1.0f, 1);
    gBreakerPulled = 0;
    // XPluginDisable tears the gate down; re-detect the aircraft so a
    // disable/enable cycle does not leave the gate permanently inactive.
    gGate.onPlaneLoaded();
    return 1;
}

PLUGIN_API void XPluginDisable(void) {
    XPLMScheduleFlightLoop(gMainLoop, 0.0f, 1);
    gGate.shutdown();
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int inMessage, void* inParam) {
    const bool userAircraftLoaded = inMessage == XPLM_MSG_PLANE_LOADED &&
                                    reinterpret_cast<intptr_t>(inParam) == XPLM_USER_AIRCRAFT;
    if (!userAircraftLoaded) return;
    gBreakerPulled = 0;
    gGate.onPlaneLoaded();
}
