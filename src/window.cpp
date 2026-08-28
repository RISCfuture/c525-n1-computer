#include "window.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "XPLMDisplay.h"
#include "XPLMUtilities.h"

#include "plugin_paths.h"
#include "segment_display.h"

namespace sfn1 {

void ensureSharedFontAtlas() {
    if (ImgWindow::sFontAtlas) return;
    ImgWindow::sFontAtlas = std::make_shared<ImgFontAtlas>();
    ImgWindow::sFontAtlas->AddFontDefault();
}

void releaseSharedFontAtlas() { ImgWindow::sFontAtlas.reset(); }

namespace {

constexpr float kBaseScale = 0.32f;  // the real unit is a ~2.6 in panel instrument
constexpr float kMinScale = 0.20f;
constexpr float kMaxScale = 1.0f;
constexpr float kPressedKnobShrink = 0.96f;  // push-in illusion while the knob is pressed
constexpr float kKnobSlewPerSecond = 14.0f;
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
constexpr double kTempSetBlinkHz = 1.5;
constexpr double kTempSetBlinkDuty = 0.6;  // fraction of each cycle at full brightness
constexpr float kTempSetBlinkDimFactor = 0.3f;
constexpr ImU32 kPlateFallbackColor = IM_COL32(43, 43, 45, 255);
constexpr ImU32 kRecessFallbackColor = IM_COL32(7, 7, 8, 255);
constexpr float kCornerZoneFraction = 0.10f;  // of the window width
constexpr float kCornerZoneMinPx = 16.0f;
constexpr float kCornerZoneMaxPx = 32.0f;
constexpr float kCursorRadius = 9.0f;
// The resize cursor is two arrowheads stacked along the drag diagonal, both
// aimed out at the corner. Distances run along that diagonal from the pointer.
constexpr float kResizeTip = 9.5f;        // the outer, larger arrowhead's point
constexpr float kResizeJoin = -1.5f;      // its base, which the inner point meets
constexpr float kResizeTail = -10.5f;     // the inner, smaller arrowhead's base
constexpr float kResizeOuterHalf = 8.5f;  // half-width of each base
constexpr float kResizeInnerHalf = 6.0f;
constexpr float kDiagonal = 0.70710678f;
constexpr ImU32 kCursorInk = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kCursorEdge = IM_COL32(0, 0, 0, 190);
constexpr ImU32 kCloseCursorFill = IM_COL32(214, 40, 40, 255);
constexpr float kCursorReach = 14.0f;  // half-extent of the largest glyph
// ImDrawList::AddLine nudges both endpoints half a pixel; the circle and
// triangle primitives take their points as given. Undo it where the two meet,
// or the cross sits low and right of the disc it is centred in.
constexpr float kLineNudge = 0.5f;

struct Frame {
    int left = 0, top = 0, right = 0, bottom = 0;
    bool plausible() const { return right > left && top > bottom; }
};

struct PixelRect {
    ImVec2 min, max;
};

struct NormRect {
    float x0, y0, x1, y1;
};

// Generous hit areas over the engraved CLB / CRU legends (fractions of the
// faceplate), sized from the label positions in assets/README.md.
constexpr NormRect kClbLabelRect{0.17f, 0.55f, 0.34f, 0.67f};
constexpr NormRect kCruLabelRect{0.63f, 0.55f, 0.80f, 0.67f};

int scaledPx(float base, float scale) { return static_cast<int>(std::lround(base * scale)); }

bool withinRect(const ImVec2& local, const NormRect& r, const ImVec2& size) {
    return local.x >= r.x0 * size.x && local.x <= r.x1 * size.x && local.y >= r.y0 * size.y &&
           local.y <= r.y1 * size.y;
}

/// X-Plane 12.2 (XPLM420) added the cockpit manipulator cursors. An older sim
/// would not recognise the codes, so it keeps the plain arrow instead.
bool simHasManipulatorCursors() {
    static const bool available = [] {
        int xplaneVersion = 0, xplmVersion = 0;
        XPLMHostApplicationID host{};
        XPLMGetVersions(&xplaneVersion, &xplmVersion, &host);
        return xplmVersion >= 420;
    }();
    return available;
}

Frame centeredDefaultFrame() {
    const Layout defaults;
    const int width = scaledPx(defaults.pngW, kBaseScale);
    const int height = scaledPx(defaults.pngH, kBaseScale);
    int left, top, right, bottom;
    XPLMGetScreenBoundsGlobal(&left, &top, &right, &bottom);
    const int cx = (left + right) / 2;
    const int cy = (top + bottom) / 2;
    return {cx - width / 2, cy + height / 2, cx + width / 2, cy - height / 2};
}

std::string settingsPath() { return pluginDir() + "/settings.ini"; }

Layout loadLayoutOrDefault() {
    if (auto layout = Layout::loadFromFile(pluginDir() + "/assets/layout.json"))
        return *layout;
    XPLMDebugString(
        "SafeFlightN1: assets/layout.json missing or malformed; using built-in layout\n");
    return {};
}

std::map<std::string, int> readIni(const std::string& path) {
    std::map<std::string, int> values;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        try {
            values[line.substr(0, eq)] = std::stoi(line.substr(eq + 1));
        } catch (...) {
            // A malformed value means that setting falls back to its default.
        }
    }
    return values;
}

void writeIni(const std::string& path, const std::map<std::string, int>& values) {
    std::ofstream file(path, std::ios::trunc);
    if (!file) return;
    file << "[window]\n";
    for (const auto& [key, value] : values) file << key << "=" << value << "\n";
}

Frame frameFromIni(const std::map<std::string, int>& ini, const std::string& prefix) {
    const auto value = [&](const std::string& key) {
        const auto it = ini.find(prefix + key);
        return it == ini.end() ? 0 : it->second;
    };
    return {value("left"), value("top"), value("right"), value("bottom")};
}

PixelRect scaledRect(const Layout::Rect& r, const ImVec2& origin, const ImVec2& size) {
    const ImVec2 min(origin.x + r.x * size.x, origin.y + r.y * size.y);
    return {min, ImVec2(min.x + r.w * size.x, min.y + r.h * size.y)};
}

/// Dismiss cursor: a red disc crossed out, outlined so it reads against both
/// the dark plate and whatever the window sits over.
void drawCloseCursor(ImDrawList* drawList, const ImVec2& at) {
    drawList->AddCircleFilled(at, kCursorRadius, kCloseCursorFill);
    drawList->AddCircle(at, kCursorRadius, kCursorEdge, 0, 1.5f);
    const ImVec2 cross(at.x - kLineNudge, at.y - kLineNudge);
    const float arm = kCursorRadius * 0.45f;
    drawList->AddLine(ImVec2(cross.x - arm, cross.y - arm),
                      ImVec2(cross.x + arm, cross.y + arm), kCursorInk, 2.0f);
    drawList->AddLine(ImVec2(cross.x - arm, cross.y + arm),
                      ImVec2(cross.x + arm, cross.y - arm), kCursorInk, 2.0f);
}

/// Resize cursor: two arrowheads pointing out along the diagonal the corner
/// pulls on, the smaller one's point meeting the larger one's base.
/// @a leftward aims it at the bottom-left corner rather than the bottom-right.
void drawResizeCursor(ImDrawList* drawList, const ImVec2& at, bool leftward) {
    const ImVec2 out(leftward ? -kDiagonal : kDiagonal, kDiagonal);
    const ImVec2 across(-out.y, out.x);
    const auto point = [&](float along, float offset) {
        return ImVec2(at.x + along * out.x + offset * across.x,
                      at.y + along * out.y + offset * across.y);
    };
    const auto arrowhead = [&](float tip, float base, float half) {
        const ImVec2 a = point(tip, 0.0f), b = point(base, half), c = point(base, -half);
        drawList->AddTriangleFilled(a, b, c, kCursorInk);
        drawList->AddTriangle(a, b, c, kCursorEdge, 1.5f);
    };
    // The inner head goes down first so the outer one paints over the point
    // they share, leaving the smaller outline no way into the larger body.
    arrowhead(kResizeJoin, kResizeTail, kResizeInnerHalf);
    arrowhead(kResizeTip, kResizeJoin, kResizeOuterHalf);
}

bool labelClicked(const char* id, const NormRect& r, const ImVec2& origin,
                  const ImVec2& size) {
    ImGui::SetCursorScreenPos(ImVec2(origin.x + r.x0 * size.x, origin.y + r.y0 * size.y));
    return ImGui::InvisibleButton(id, ImVec2((r.x1 - r.x0) * size.x, (r.y1 - r.y0) * size.y));
}

struct DigitText {
    std::string text;
    bool decimal = false;
};

std::string signedIntText(double value) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%3d",
                  static_cast<int>(std::lround(std::clamp(value, -99.0, 999.0))));
    return buf;
}

DigitText n1Text(double value) {
    if (value >= 99.95) return {signedIntText(value), false};
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%3d",
                  static_cast<int>(std::lround(std::clamp(value, 0.0, 99.9) * 10.0)));
    return {buf, true};
}

DigitText digitsFor(const Output& output) {
    switch (output.state) {
        case DisplayState::SelfTest888: return {"888", false};
        case DisplayState::Dashes: return {"---", false};
        case DisplayState::Rat:
        case DisplayState::TempSet: return {signedIntText(output.value), false};
        case DisplayState::N1: return n1Text(output.value);
        // Fail keeps the display energized but showing nothing, so the unlit
        // ghost segments stay visible; Off skips rendering entirely.
        case DisplayState::Fail:
        case DisplayState::Off: break;
    }
    return {"   ", false};
}

}  // namespace

N1SettingWindow::N1SettingWindow(Actions actions)
    : ImgWindow(centeredDefaultFrame().left, centeredDefaultFrame().top,
                centeredDefaultFrame().right, centeredDefaultFrame().bottom,
                xplm_WindowDecorationSelfDecoratedResizable, xplm_WindowLayerFloatingWindows),
      layout_(loadLayoutOrDefault()),
      actions_(std::move(actions)) {
    SetWindowTitle("N1 SETTING");
    SetWindowResizingLimits(
        scaledPx(layout_.pngW, kMinScale), scaledPx(layout_.pngH, kMinScale),
        scaledPx(layout_.pngW, kMaxScale), scaledPx(layout_.pngH, kMaxScale));
    knobAngleDeg_ = layout_.knob.togaDeg;
    restoreGeometry();
    enforceAspect();
}

N1SettingWindow::~N1SettingWindow() {
    // Failing to save the window position is not worth terminating the sim for,
    // which is what an exception escaping a destructor would do.
    try {
        persistGeometry();
    } catch (...) {
    }
}

void N1SettingWindow::toggle() { SetVisible(!GetVisible()); }

void N1SettingWindow::showValues(const InputSnapshot& input, const Output& output,
                                 float brightness) {
    input_ = input;
    output_ = output;
    brightness_ = std::clamp(brightness, 0.0f, 1.0f);
    persistOnExternalClose();
}

void N1SettingWindow::SetVisible(bool inIsVisible) {
    if (!inIsVisible && GetVisible()) persistGeometry();
    ImgWindow::SetVisible(inIsVisible);
    wasVisible_ = GetVisible();
}

// cppcheck-suppress unusedFunction  // ImgWindow calls this override
ImGuiWindowFlags_ N1SettingWindow::beforeBegin() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    return static_cast<ImGuiWindowFlags_>(ImGuiWindowFlags_NoScrollbar |
                                          ImGuiWindowFlags_NoScrollWithMouse |
                                          ImGuiWindowFlags_NoBackground);
}

// cppcheck-suppress unusedFunction  // ImgWindow calls this override
void N1SettingWindow::buildInterface() {
    ImGui::PopStyleVar(2);
    loadAssetsOnce();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    drawFaceplate(drawList, origin, size);
    drawDisplay(drawList, origin, size);
    updateKnobAngle();
    drawKnob(drawList, origin, size);
    handleKnobMouse(origin, size);
    handleModeLabelClicks(origin, size);
    handleCorners(drawList, origin, size);
}

// cppcheck-suppress unusedFunction  // ImgWindow calls this override
ImgWindow::DragTy N1SettingWindow::dragTargetAt(int x, int y) const {
    DragTy what;
    const ImVec2 local(static_cast<float>(x), static_cast<float>(y));
    const ImVec2 size = windowSize();
    if (controlAt(local, size) != Control::None) return what;  // the controls win
    switch (cornerAt(local, size)) {
        case Corner::Close: break;  // dismissed on click; nothing to drag
        case Corner::ResizeLeft: what.left = what.bottom = true; break;
        case Corner::ResizeRight: what.right = what.bottom = true; break;
        case Corner::None: what.wnd = true; break;
    }
    return what;
}

// cppcheck-suppress unusedFunction  // ImgWindow calls this override
void N1SettingWindow::constrainDrag(const DragTy& what, int& left, int& top, int& right,
                                    int& bottom) const {
    if (what.wnd) return;
    const float scale =
        fittedScale(static_cast<float>(right - left), static_cast<float>(top - bottom));
    const int width = scaledPx(layout_.pngW, scale);
    const int height = scaledPx(layout_.pngH, scale);
    if (what.left)
        left = right - width;
    else
        right = left + width;
    if (what.top)
        top = bottom + height;
    else
        bottom = top - height;
}

// cppcheck-suppress unusedFunction  // ImgWindow calls this override
XPLMCursorStatus N1SettingWindow::cursorAt(int x, int y) {
    ImGui::GetIO().MouseDrawCursor = false;
    // Popped out or in VR the frame is not ours to close, move or resize.
    if (!IsInsideSim()) return xplm_CursorDefault;
    const ImVec2 local(static_cast<float>(x), static_cast<float>(y));
    const ImVec2 size = windowSize();
    // The corners are drawn in buildInterface(), so the OS cursor gets out of
    // the way; the controls borrow the sim's own cockpit manipulator cursors.
    if (activeCorner(local, size) != Corner::None) return xplm_CursorHidden;
    if (!simHasManipulatorCursors()) return xplm_CursorDefault;
    switch (controlAt(local, size)) {
        case Control::Knob: return xplm_CursorButton;
        case Control::ClbLegend: return xplm_CursorRotateSmallLeft;
        case Control::CruLegend: return xplm_CursorRotateSmallRight;
        case Control::None: break;
    }
    return xplm_CursorDefault;
}

void N1SettingWindow::handleCorners(ImDrawList* drawList, const ImVec2& origin,
                                    const ImVec2& size) {
    const std::optional<ImVec2> pointer = pointerInWindow();
    if (!pointer) return;
    const Corner corner = activeCorner(*pointer, size);
    if (corner == Corner::None) return;
    const ImVec2 at(origin.x + pointer->x, origin.y + pointer->y);
    // A cursor overhangs whatever it points at. The window's own clip rect would
    // slice the glyph off at the very corner it belongs to, so give it one of
    // its own: ImgWindow scissors straight from this rect, without clamping it
    // to the window, which lets the glyph finish outside the frame.
    drawList->PushClipRect(ImVec2(at.x - kCursorReach, at.y - kCursorReach),
                           ImVec2(at.x + kCursorReach, at.y + kCursorReach));
    switch (corner) {
        case Corner::Close:
            drawCloseCursor(drawList, at);
            if (ImGui::IsMouseClicked(0)) SetVisible(false);
            break;
        case Corner::ResizeLeft: drawResizeCursor(drawList, at, true); break;
        case Corner::ResizeRight: drawResizeCursor(drawList, at, false); break;
        case Corner::None: break;
    }
    drawList->PopClipRect();
}

N1SettingWindow::Corner N1SettingWindow::cornerAt(const ImVec2& local,
                                                  const ImVec2& size) const {
    const float zone =
        std::clamp(size.x * kCornerZoneFraction, kCornerZoneMinPx, kCornerZoneMaxPx);
    const bool nearLeft = local.x <= zone;
    if (local.y <= zone) return nearLeft ? Corner::Close : Corner::None;
    if (local.y < size.y - zone) return Corner::None;
    if (nearLeft) return Corner::ResizeLeft;
    return local.x >= size.x - zone ? Corner::ResizeRight : Corner::None;
}

/// The corner the pointer acts on, which stays the one a resize started from
/// even as the drag runs away from it.
N1SettingWindow::Corner N1SettingWindow::activeCorner(const ImVec2& local,
                                                      const ImVec2& size) const {
    if (dragWhat.bottom) return dragWhat.left ? Corner::ResizeLeft : Corner::ResizeRight;
    if (controlAt(local, size) != Control::None) return Corner::None;
    return cornerAt(local, size);
}

/// The knob is its face, not the square that bounds it, so the plate stays
/// draggable right up to the shaft.
N1SettingWindow::Control N1SettingWindow::controlAt(const ImVec2& local,
                                                    const ImVec2& size) const {
    const ImVec2 center = knobCenter(ImVec2(0.0f, 0.0f), size);
    const float dx = local.x - center.x;
    const float dy = local.y - center.y;
    const float radius = knobRadius(size);
    if (dx * dx + dy * dy <= radius * radius) return Control::Knob;
    if (withinRect(local, kClbLabelRect, size)) return Control::ClbLegend;
    if (withinRect(local, kCruLabelRect, size)) return Control::CruLegend;
    return Control::None;
}

/// Pointer position in window coordinates, or nullopt when it is elsewhere.
/// Taken from the sim rather than from ImGui, whose copy goes stale as soon as
/// the pointer leaves the window and stops delivering events.
std::optional<ImVec2> N1SettingWindow::pointerInWindow() const {
    if (!IsInsideSim()) return std::nullopt;
    int left, top, right, bottom;
    GetWindowGeometry(left, top, right, bottom);
    int x, y;
    XPLMGetMouseLocationGlobal(&x, &y);
    if (x < left || x > right || y < bottom || y > top) return std::nullopt;
    return ImVec2(static_cast<float>(x - left), static_cast<float>(top - y));
}

ImVec2 N1SettingWindow::windowSize() const {
    int left, top, right, bottom;
    GetCurrentWindowGeometry(left, top, right, bottom);
    return ImVec2(static_cast<float>(right - left), static_cast<float>(top - bottom));
}

void N1SettingWindow::loadAssetsOnce() {
    if (assetsLoadTried_) return;
    assetsLoadTried_ = true;
    const std::string dir = pluginDir() + "/assets/";
    faceplate_ = Texture::loadPng(dir + "faceplate.png");
    knob_ = Texture::loadPng(dir + "knob.png");
    if (!faceplate_ || !knob_)
        XPLMDebugString(
            "SafeFlightN1: faceplate art missing from assets/; "
            "rendering flat fallback\n");
}

void N1SettingWindow::drawFaceplate(ImDrawList* drawList, const ImVec2& origin,
                                    const ImVec2& size) {
    const ImVec2 corner(origin.x + size.x, origin.y + size.y);
    if (faceplate_) {
        drawList->AddImage(faceplate_->id(), origin, corner);
        return;
    }
    drawList->AddRectFilled(origin, corner, kPlateFallbackColor);
    const PixelRect recess = scaledRect(layout_.displayWindow, origin, size);
    drawList->AddRectFilled(recess.min, recess.max, kRecessFallbackColor);
}

void N1SettingWindow::drawDisplay(ImDrawList* drawList, const ImVec2& origin,
                                  const ImVec2& size) {
    if (output_.state == DisplayState::Off) return;
    const PixelRect rect = scaledRect(layout_.displayWindow, origin, size);
    const DigitText digits = digitsFor(output_);
    const segment_display::Style style{layout_.digitCells, layout_.decimalAfter,
                                       displayBrightness()};
    segment_display::draw(drawList, rect.min, rect.max, digits.text.c_str(), digits.decimal,
                          style);
}

void N1SettingWindow::drawKnob(ImDrawList* drawList, const ImVec2& origin,
                               const ImVec2& size) {
    if (!knob_) return;
    const ImVec2 center = knobCenter(origin, size);
    const float half = knobRadius(size) * (input_.knobPressed ? kPressedKnobShrink : 1.0f);
    const float angle = knobAngleDeg_ * kDegToRad;
    const float cosA = std::cos(angle);
    const float sinA = std::sin(angle);
    const auto corner = [&](float x, float y) {
        return ImVec2(center.x + (x * cosA - y * sinA) * half,
                      center.y + (x * sinA + y * cosA) * half);
    };
    drawList->AddImageQuad(knob_->id(), corner(-1.0f, -1.0f), corner(1.0f, -1.0f),
                           corner(1.0f, 1.0f), corner(-1.0f, 1.0f));
}

void N1SettingWindow::updateKnobAngle() {
    const float target = modeAngleDeg(output_.mode);
    const float blend = std::min(1.0f, ImGui::GetIO().DeltaTime * kKnobSlewPerSecond);
    knobAngleDeg_ += (target - knobAngleDeg_) * blend;
}

float N1SettingWindow::modeAngleDeg(Mode mode) const {
    switch (mode) {
        case Mode::Clb: return layout_.knob.clbDeg;
        case Mode::ToGa: return layout_.knob.togaDeg;
        case Mode::Cru: return layout_.knob.cruDeg;
    }
    return layout_.knob.togaDeg;
}

float N1SettingWindow::displayBrightness() const {
    if (output_.state != DisplayState::TempSet) return brightness_;
    const double period = 1.0 / kTempSetBlinkHz;
    const bool dimPhase = std::fmod(ImGui::GetTime(), period) >= period * kTempSetBlinkDuty;
    return dimPhase ? brightness_ * kTempSetBlinkDimFactor : brightness_;
}

/// The knob only ever presses under the mouse, matching a real shaft you push
/// straight in. Turning it is the wheel's job, or the legends either side.
void N1SettingWindow::handleKnobMouse(const ImVec2& origin, const ImVec2& size) {
    const ImVec2 center = knobCenter(origin, size);
    const float radius = knobRadius(size);
    ImGui::SetCursorScreenPos(ImVec2(center.x - radius, center.y - radius));
    ImGui::InvisibleButton("knob", ImVec2(radius * 2.0f, radius * 2.0f));
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float dx = mouse.x - center.x;
    const float dy = mouse.y - center.y;
    if (ImGui::IsItemActivated() && dx * dx + dy * dy <= radius * radius)
        knobHeldByMouse_ = true;
    if (!ImGui::IsItemActive()) knobHeldByMouse_ = false;
    handleKnobWheel(ImGui::IsItemHovered() || knobHeldByMouse_);
}

void N1SettingWindow::handleKnobWheel(bool overKnob) {
    if (!overKnob) {
        wheelAccum_ = 0.0f;
        return;
    }
    wheelAccum_ += ImGui::GetIO().MouseWheel;
    const int detents = static_cast<int>(wheelAccum_);
    if (detents == 0) return;
    wheelAccum_ -= static_cast<float>(detents);
    if (input_.knobPressed) {
        if (actions_.bumpTemp) actions_.bumpTemp(static_cast<double>(detents));
        return;
    }
    for (int i = 0; i < std::abs(detents); ++i) bumpModeBy(detents > 0 ? +1 : -1);
}

void N1SettingWindow::handleModeLabelClicks(const ImVec2& origin, const ImVec2& size) {
    if (labelClicked("clb_label", kClbLabelRect, origin, size)) bumpModeBy(-1);
    if (labelClicked("cru_label", kCruLabelRect, origin, size)) bumpModeBy(+1);
}

void N1SettingWindow::bumpModeBy(int delta) {
    if (actions_.bumpMode) actions_.bumpMode(delta);
}

ImVec2 N1SettingWindow::knobCenter(const ImVec2& origin, const ImVec2& size) const {
    return ImVec2(origin.x + layout_.knob.cx * size.x, origin.y + layout_.knob.cy * size.y);
}

float N1SettingWindow::knobRadius(const ImVec2& size) const { return layout_.knob.r * size.x; }

/// Snaps restored geometry onto the faceplate aspect, anchored at its top-left
/// corner: a hand-edited settings.ini can hold anything. Resize drags stay on
/// aspect by construction, so nothing has to correct them afterwards.
void N1SettingWindow::enforceAspect() {
    if (IsInVR()) return;
    const bool popped = IsPoppedOut();
    int left, top, right, bottom;
    if (popped)
        GetWindowGeometryOS(left, top, right, bottom);
    else
        GetWindowGeometry(left, top, right, bottom);
    const int width = right - left;
    const int height = top - bottom;
    if (width <= 0 || height <= 0) return;
    const float scale = fittedScale(static_cast<float>(width), static_cast<float>(height));
    const int idealW = scaledPx(layout_.pngW, scale);
    const int idealH = scaledPx(layout_.pngH, scale);
    if (std::abs(width - idealW) <= 1 && std::abs(height - idealH) <= 1) return;
    if (popped)
        SetWindowGeometryOS(left, top, left + idealW, top - idealH);
    else
        SetWindowGeometry(left, top, left + idealW, top - idealH);
}

/// The on-aspect scale nearest the dragged size. Projecting onto the aspect
/// line rather than picking a driving axis lets the corner slide along the
/// faceplate's diagonal, following the pointer both ways at once, and the
/// projection composes: a drag ends where the whole pointer travel points.
float N1SettingWindow::fittedScale(float width, float height) const {
    const float w = layout_.pngW;
    const float h = layout_.pngH;
    return std::clamp((width * w + height * h) / (w * w + h * h), kMinScale, kMaxScale);
}

void N1SettingWindow::restoreGeometry() {
    const auto ini = readIni(settingsPath());
    const Frame sim = frameFromIni(ini, "");
    if (!sim.plausible()) return;
    SetWindowGeometry(sim.left, sim.top, sim.right, sim.bottom);
    const auto popped = ini.find("popped");
    if (popped == ini.end() || popped->second == 0) return;
    SetWindowPositioningMode(xplm_WindowPopOut, -1);
    const Frame os = frameFromIni(ini, "os_");
    if (os.plausible()) SetWindowGeometryOS(os.left, os.top, os.right, os.bottom);
}

void N1SettingWindow::persistGeometry() {
    std::map<std::string, int> ini;
    Frame sim;
    GetWindowGeometry(sim.left, sim.top, sim.right, sim.bottom);
    ini["left"] = sim.left;
    ini["top"] = sim.top;
    ini["right"] = sim.right;
    ini["bottom"] = sim.bottom;
    ini["popped"] = IsPoppedOut() ? 1 : 0;
    if (IsPoppedOut()) {
        Frame os;
        GetWindowGeometryOS(os.left, os.top, os.right, os.bottom);
        ini["os_left"] = os.left;
        ini["os_top"] = os.top;
        ini["os_right"] = os.right;
        ini["os_bottom"] = os.bottom;
    }
    writeIni(settingsPath(), ini);
}

void N1SettingWindow::persistOnExternalClose() {
    const bool visible = GetVisible();
    if (wasVisible_ && !visible) persistGeometry();
    wasVisible_ = visible;
}

}  // namespace sfn1
