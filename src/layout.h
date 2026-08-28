#pragma once

#include <optional>
#include <string>

namespace sfn1 {

/// Faceplate geometry parsed from assets/layout.json. Coordinates are
/// normalized fractions of the faceplate PNG's width/height so the art and
/// code stay decoupled (schema in docs/CONTRACTS.md). Defaults match the
/// shipped art and act as a fallback when the file is missing.
struct Layout {
    /// Normalized rectangle (fractions of PNG width/height).
    struct Rect {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    };

    /// Mode knob placement and detent pointer angles.
    struct Knob {
        float cx = 0.485f;      ///< Shaft center, fraction of PNG width.
        float cy = 0.7714f;     ///< Shaft center, fraction of PNG height.
        float r = 0.1208f;      ///< Knob radius, fraction of PNG width.
        float clbDeg = -40.0f;  ///< Pointer angle at the CLB detent.
        float togaDeg = 0.0f;   ///< Pointer angle at the TO/GA detent.
        float cruDeg = 40.0f;   ///< Pointer angle at the CRU detent.
    };

    float pngW = 1200.0f;  ///< Faceplate PNG width in pixels.
    float pngH = 936.0f;   ///< Faceplate PNG height in pixels.
    Rect displayWindow{0.2933f, 0.2212f, 0.365f, 0.2756f};
    Knob knob{};
    int digitCells = 3;    ///< Digit cells across the segment display.
    int decimalAfter = 2;  ///< Decimal point sits after this 1-based cell.

    /// Parses exactly the layout.json schema; nullopt if any key is missing
    /// or malformed.
    static std::optional<Layout> parse(const std::string& json);

    /// Reads and parses the layout file at path; nullopt if unreadable or
    /// malformed.
    static std::optional<Layout> loadFromFile(const std::string& path);
};

}  // namespace sfn1
