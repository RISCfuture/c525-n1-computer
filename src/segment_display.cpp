#include "segment_display.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace sfn1::segment_display {
namespace {

constexpr float kCellWidthRatio = 0.58f;    // cell width as a fraction of cell height
constexpr float kCellGapRatio = 0.24f;      // gap between cells
constexpr float kDecimalSlotRatio = 0.18f;  // extra room reserved for the decimal point
constexpr float kThicknessRatio = 0.14f;    // segment thickness
constexpr float kSegmentGapRatio = 0.035f;  // gap between adjacent segment tips
constexpr float kSkewRatio = 0.07f;         // italic slant of the digits
constexpr float kHeightFillRatio = 0.66f;   // digit height as a fraction of the rect height
constexpr float kWidthFillRatio = 0.88f;    // usable fraction of the rect width
constexpr float kDecimalRadiusRatio = 0.07f;

constexpr float kGhostFraction = 0.10f;  // unlit segment intensity, relative to lit

enum SegmentBit : unsigned {
    kSegA = 1u << 0,  // top
    kSegB = 1u << 1,  // top right
    kSegC = 1u << 2,  // bottom right
    kSegD = 1u << 3,  // bottom
    kSegE = 1u << 4,  // bottom left
    kSegF = 1u << 5,  // top left
    kSegG = 1u << 6,  // middle
};

unsigned segmentsFor(char c) {
    switch (c) {
        case '0': return kSegA | kSegB | kSegC | kSegD | kSegE | kSegF;
        case '1': return kSegB | kSegC;
        case '2': return kSegA | kSegB | kSegG | kSegE | kSegD;
        case '3': return kSegA | kSegB | kSegG | kSegC | kSegD;
        case '4': return kSegF | kSegG | kSegB | kSegC;
        case '5': return kSegA | kSegF | kSegG | kSegC | kSegD;
        case '6': return kSegA | kSegF | kSegG | kSegE | kSegD | kSegC;
        case '7': return kSegA | kSegB | kSegC;
        case '8': return kSegA | kSegB | kSegC | kSegD | kSegE | kSegF | kSegG;
        case '9': return kSegA | kSegB | kSegC | kSegD | kSegF | kSegG;
        case '-': return kSegG;
        default: return 0;
    }
}

ImU32 amber(float brightness, float alpha) {
    const auto scaled = [brightness](int channel) {
        return static_cast<int>(std::lround(channel * brightness));
    };
    return IM_COL32(scaled(255), scaled(178), scaled(48),
                    static_cast<int>(std::lround(alpha * 255.0f)));
}

using Poly = std::array<ImVec2, 6>;

/// One digit cell's box: local coordinates are y-down with (0,0) at the
/// unskewed top-left; at() applies the italic slant.
struct CellFrame {
    ImVec2 origin;
    float w, h, skew;

    ImVec2 at(float x, float y) const {
        return ImVec2(origin.x + x + skew * (h - y) / h, origin.y + y);
    }
};

Poly horizontalSegment(const CellFrame& frame, float xa, float xb, float y) {
    const float half = frame.h * kThicknessRatio * 0.5f;
    const float x0 = xa + frame.h * kSegmentGapRatio;
    const float x1 = xb - frame.h * kSegmentGapRatio;
    return {frame.at(x0, y), frame.at(x0 + half, y - half), frame.at(x1 - half, y - half),
            frame.at(x1, y), frame.at(x1 - half, y + half), frame.at(x0 + half, y + half)};
}

Poly verticalSegment(const CellFrame& frame, float x, float ya, float yb) {
    const float half = frame.h * kThicknessRatio * 0.5f;
    const float y0 = ya + frame.h * kSegmentGapRatio;
    const float y1 = yb - frame.h * kSegmentGapRatio;
    return {frame.at(x, y0), frame.at(x + half, y0 + half), frame.at(x + half, y1 - half),
            frame.at(x, y1), frame.at(x - half, y1 - half), frame.at(x - half, y0 + half)};
}

std::array<Poly, 7> segmentPolys(const CellFrame& frame) {
    const float mid = frame.h * 0.5f;
    return {
        horizontalSegment(frame, 0.0f, frame.w, 0.0f),     // A
        verticalSegment(frame, frame.w, 0.0f, mid),        // B
        verticalSegment(frame, frame.w, mid, frame.h),     // C
        horizontalSegment(frame, 0.0f, frame.w, frame.h),  // D
        verticalSegment(frame, 0.0f, mid, frame.h),        // E
        verticalSegment(frame, 0.0f, 0.0f, mid),           // F
        horizontalSegment(frame, 0.0f, frame.w, mid),      // G
    };
}

Poly inflated(const Poly& poly, float factor) {
    ImVec2 center(0.0f, 0.0f);
    for (const ImVec2& p : poly) {
        center.x += p.x;
        center.y += p.y;
    }
    center.x /= poly.size();
    center.y /= poly.size();
    Poly out;
    for (size_t i = 0; i < poly.size(); ++i)
        out[i] = ImVec2(center.x + (poly[i].x - center.x) * factor,
                        center.y + (poly[i].y - center.y) * factor);
    return out;
}

void addPoly(ImDrawList* drawList, const Poly& poly, ImU32 color) {
    drawList->AddConvexPolyFilled(poly.data(), static_cast<int>(poly.size()), color);
}

/// Unlit segments glow faintly at a fixed fraction of the lit intensity, so the
/// readout keeps its contrast when the panel dims.
ImU32 ghost(float brightness) { return amber(brightness * kGhostFraction, 1.0f); }

void drawLitSegment(ImDrawList* drawList, const Poly& poly, float brightness) {
    addPoly(drawList, inflated(poly, 1.45f), amber(brightness, 0.10f));
    addPoly(drawList, inflated(poly, 1.16f), amber(brightness, 0.30f));
    addPoly(drawList, poly, amber(brightness, 1.0f));
}

void drawCell(ImDrawList* drawList, const CellFrame& frame, char c, float brightness) {
    const std::array<Poly, 7> polys = segmentPolys(frame);
    for (const Poly& poly : polys) addPoly(drawList, poly, ghost(brightness));
    const unsigned lit = segmentsFor(c);
    for (size_t s = 0; s < polys.size(); ++s)
        if (lit & (1u << s)) drawLitSegment(drawList, polys[s], brightness);
}

void drawDecimal(ImDrawList* drawList, const ImVec2& center, float radius, bool lit,
                 float brightness) {
    drawList->AddCircleFilled(center, radius, ghost(brightness));
    if (!lit) return;
    drawList->AddCircleFilled(center, radius * 1.45f, amber(brightness, 0.10f));
    drawList->AddCircleFilled(center, radius * 1.16f, amber(brightness, 0.30f));
    drawList->AddCircleFilled(center, radius, amber(brightness, 1.0f));
}

std::string rightAligned(const char* text, int cells) {
    std::string s(text ? text : "");
    if (static_cast<int>(s.size()) > cells) s.erase(0, s.size() - cells);
    return std::string(cells - s.size(), ' ') + s;
}

}  // namespace

void draw(ImDrawList* drawList, const ImVec2& rectMin, const ImVec2& rectMax, const char* text,
          bool withDecimal, const Style& style) {
    const int cells = std::max(1, style.digitCells);
    const float brightness = std::clamp(style.brightness, 0.0f, 1.0f);
    const float rectW = rectMax.x - rectMin.x;
    const float rectH = rectMax.y - rectMin.y;
    const bool hasDecimalSlot = style.decimalAfter >= 1 && style.decimalAfter < cells;
    const float widthUnits = cells * kCellWidthRatio + (cells - 1) * kCellGapRatio +
                             (hasDecimalSlot ? kDecimalSlotRatio : 0.0f) + kSkewRatio;
    const float h = std::min(rectH * kHeightFillRatio, rectW * kWidthFillRatio / widthUnits);
    const float w = h * kCellWidthRatio;
    const float gap = h * kCellGapRatio;
    const float blockW = h * widthUnits;
    const float bottom = rectMin.y + (rectH + h) * 0.5f;

    const std::string padded = rightAligned(text, cells);
    ImVec2 cursor(rectMin.x + (rectW - blockW) * 0.5f, rectMin.y + (rectH - h) * 0.5f);
    for (int i = 0; i < cells; ++i) {
        drawCell(drawList, {cursor, w, h, h * kSkewRatio}, padded[i], brightness);
        cursor.x += w + gap;
        if (hasDecimalSlot && i + 1 == style.decimalAfter) {
            const float radius = h * kDecimalRadiusRatio;
            drawDecimal(drawList, ImVec2(cursor.x - gap * 0.5f + radius, bottom - radius),
                        radius, withDecimal, brightness);
            cursor.x += h * kDecimalSlotRatio;
        }
    }
}

}  // namespace sfn1::segment_display
