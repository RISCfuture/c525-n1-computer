#pragma once

#include <functional>
#include <optional>

#include "ImgWindow.h"
#include "layout.h"
#include "sim_inputs.h"
#include "textures.h"

namespace sfn1 {

/// Popup rendering of the C-12732-1 faceplate: PNG plate art, amber
/// seven-segment readout, and a mode knob that presses under the mouse and
/// turns to the scroll wheel or a click on a legend. The window wears no title
/// bar, so its corners carry the
/// window gestures: the top-left one dismisses it, the bottom two resize it on
/// the faceplate's aspect ratio, and dragging anywhere off a control moves it.
/// Window geometry (including pop-out state) persists to settings.ini next to
/// the plugin across sessions.
class N1SettingWindow : public ImgWindow {
public:
    /// Device actions the window's mouse gestures trigger.
    struct Actions {
        std::function<void(int)> bumpMode;     ///< Move the mode knob one detent (+1 or -1).
        std::function<void(double)> bumpTemp;  ///< Adjust the selected temperature in degC.
    };

    explicit N1SettingWindow(Actions actions);
    ~N1SettingWindow() override;

    /// Shows the window if hidden, hides it if visible.
    void toggle();

    /// Receives this frame's device input/output and the 0-1 panel-light
    /// factor applied to the segment brightness.
    void showValues(const InputSnapshot& input, const Output& output, float brightness);

    /// True while the mouse holds the knob's center pressed.
    bool knobHeldByMouse() const { return knobHeldByMouse_; }

    void SetVisible(bool inIsVisible) override;

protected:
    ImGuiWindowFlags_ beforeBegin() override;
    void buildInterface() override;
    DragTy dragTargetAt(int x, int y) const override;
    void constrainDrag(const DragTy& what, int& left, int& top, int& right,
                       int& bottom) const override;
    XPLMCursorStatus cursorAt(int x, int y) override;

private:
    /// Window corner under the pointer, each carrying its own gesture.
    enum class Corner { None, Close, ResizeLeft, ResizeRight };

    /// Device control under the pointer, each carrying its own cursor.
    enum class Control { None, Knob, ClbLegend, CruLegend };

    void restoreGeometry();
    void persistGeometry();
    void persistOnExternalClose();
    void enforceAspect();
    float fittedScale(float width, float height) const;
    void loadAssetsOnce();

    void drawFaceplate(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size);
    void drawDisplay(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size);
    void drawKnob(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size);
    void updateKnobAngle();
    float modeAngleDeg(Mode mode) const;
    float displayBrightness() const;

    void handleKnobMouse(const ImVec2& origin, const ImVec2& size);
    void handleKnobWheel(bool overKnob);
    void handleModeLabelClicks(const ImVec2& origin, const ImVec2& size);
    void bumpModeBy(int delta);
    ImVec2 knobCenter(const ImVec2& origin, const ImVec2& size) const;
    float knobRadius(const ImVec2& size) const;

    void handleCorners(ImDrawList* drawList, const ImVec2& origin, const ImVec2& size);
    Corner cornerAt(const ImVec2& local, const ImVec2& size) const;
    Corner activeCorner(const ImVec2& local, const ImVec2& size) const;
    Control controlAt(const ImVec2& local, const ImVec2& size) const;
    std::optional<ImVec2> pointerInWindow() const;
    ImVec2 windowSize() const;

    Layout layout_;
    Actions actions_;
    std::optional<Texture> faceplate_;
    std::optional<Texture> knob_;
    bool assetsLoadTried_ = false;
    InputSnapshot input_{};
    Output output_{DisplayState::Off, 0.0, Mode::ToGa};
    float brightness_ = 1.0f;
    float knobAngleDeg_ = 0.0f;
    float wheelAccum_ = 0.0f;
    bool knobHeldByMouse_ = false;
    bool wasVisible_ = false;
};

/// Creates the font atlas that every ImgWindow shares. ImgWindow hands its
/// static atlas to each context as it is constructed, so this must run before
/// the first window exists.
void ensureSharedFontAtlas();

/// Releases the shared font atlas. Call after the last window is destroyed.
void releaseSharedFontAtlas();

}  // namespace sfn1
