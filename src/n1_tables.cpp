#include "n1_tables.h"

#include <algorithm>
#include <cstdlib>

namespace sfn1 {
namespace {

std::string trimmed(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::optional<double> parseNumber(const std::string& cell) {
    const std::string text = trimmed(cell);
    if (text.empty()) return std::nullopt;
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size()) return std::nullopt;
    return value;
}

std::vector<std::string> nonEmptyLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        const std::string line =
            text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!trimmed(line).empty()) lines.push_back(line);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return lines;
}

std::vector<std::string> splitRow(const std::string& line) {
    std::vector<std::string> cells;
    size_t start = 0;
    while (true) {
        const size_t comma = line.find(',', start);
        cells.push_back(line.substr(
            start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) return cells;
        start = comma + 1;
    }
}

bool strictlyAscending(const std::vector<double>& values) {
    return std::adjacent_find(values.begin(), values.end(),
                              [](double a, double b) { return a >= b; }) == values.end();
}

/// Index pair bracketing v on the axis, with the interpolation fraction
/// between them (lo == hi and t == 0 exactly on a grid point).
struct Span {
    size_t lo, hi;
    double t;
};

/// Whether a value below the axis minimum still has a defensible reading.
enum class BelowMinimum { NoData, UseFirstGridline };

/// Nullopt when v lies outside the charted axis: the device has no data there,
/// and extrapolating would present an uncharted thrust setting as a real one.
std::optional<Span> spanOn(const std::vector<double>& axis, double v, BelowMinimum below) {
    if (v < axis.front()) {
        if (below == BelowMinimum::NoData) return std::nullopt;
        return Span{0, 0, 0.0};
    }
    if (v > axis.back()) return std::nullopt;
    if (v == axis.front()) return Span{0, 0, 0.0};
    if (v == axis.back()) return Span{axis.size() - 1, axis.size() - 1, 0.0};
    const auto above = std::upper_bound(axis.begin(), axis.end(), v);
    const size_t hi = static_cast<size_t>(above - axis.begin());
    const size_t lo = hi - 1;
    if (axis[lo] == v) return Span{lo, lo, 0.0};
    return Span{lo, hi, (v - axis[lo]) / (axis[hi] - axis[lo])};
}

double lerp(double a, double b, double t) { return a + (b - a) * t; }

}  // namespace

std::optional<N1Table> N1Table::loadFromCsv(const std::string& csvText) {
    const std::vector<std::string> lines = nonEmptyLines(csvText);
    if (lines.size() < 2) return std::nullopt;

    const std::vector<std::string> header = splitRow(lines.front());
    if (header.size() < 2) return std::nullopt;

    N1Table table;
    for (size_t col = 1; col < header.size(); ++col) {
        const auto alt = parseNumber(header[col]);
        if (!alt) return std::nullopt;
        table.altsFt_.push_back(*alt);
    }
    if (!strictlyAscending(table.altsFt_)) return std::nullopt;

    for (size_t rowIndex = 1; rowIndex < lines.size(); ++rowIndex) {
        const std::vector<std::string> row = splitRow(lines[rowIndex]);
        if (row.size() != header.size()) return std::nullopt;
        const auto oat = parseNumber(row.front());
        if (!oat) return std::nullopt;
        table.oatsC_.push_back(*oat);

        std::vector<std::optional<double>> cells;
        for (size_t col = 1; col < row.size(); ++col) {
            if (trimmed(row[col]).empty()) {
                cells.push_back(std::nullopt);
            } else {
                const auto value = parseNumber(row[col]);
                if (!value) return std::nullopt;
                cells.push_back(value);
            }
        }
        table.cells_.push_back(std::move(cells));
    }
    if (!strictlyAscending(table.oatsC_)) return std::nullopt;
    return table;
}

std::optional<double> N1Table::lookup(double oatC, double paFt) const {
    // A schedule's lowest altitude line is its sea-level datum, and pressure
    // altitude goes slightly negative whenever the QNH is above standard, so
    // reading the bottom line there is ordinary chart practice rather than
    // extrapolation. Every other overrun is genuinely off-chart.
    const std::optional<Span> row = spanOn(oatsC_, oatC, BelowMinimum::NoData);
    const std::optional<Span> col = spanOn(altsFt_, paFt, BelowMinimum::UseFirstGridline);
    if (!row || !col) return std::nullopt;

    const auto& topLeft = cells_[row->lo][col->lo];
    const auto& topRight = cells_[row->lo][col->hi];
    const auto& bottomLeft = cells_[row->hi][col->lo];
    const auto& bottomRight = cells_[row->hi][col->hi];
    if (!topLeft || !topRight || !bottomLeft || !bottomRight) return std::nullopt;

    const double top = lerp(*topLeft, *topRight, col->t);
    const double bottom = lerp(*bottomLeft, *bottomRight, col->t);
    return lerp(top, bottom, row->t);
}

}  // namespace sfn1
