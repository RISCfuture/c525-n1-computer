#pragma once

#include "imgui.h"

namespace sfn1::segment_display {

/// Cell layout and rendering parameters for the seven-segment readout.
struct Style {
    int digitCells = 3;       ///< Number of digit cells across the display.
    int decimalAfter = 2;     ///< Decimal point sits after this 1-based cell.
    float brightness = 1.0f;  ///< 0-1 multiplier applied to lit segments.
};

/// Draws text (characters '0'-'9', '-' and ' ') right-aligned across
/// style.digitCells cells inside [rectMin, rectMax] using draw-list convex
/// polygons: faint ghost segments behind amber lit ones with a soft layered
/// glow. When withDecimal is true, the decimal point after cell
/// style.decimalAfter is lit as well.
void draw(ImDrawList* drawList, const ImVec2& rectMin, const ImVec2& rectMax, const char* text,
          bool withDecimal, const Style& style);

}  // namespace sfn1::segment_display
