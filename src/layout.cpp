#include "layout.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>

namespace sfn1 {
namespace {

bool startsNumber(char c) {
    return c == '-' || c == '+' || c == '.' || (c >= '0' && c <= '9');
}

bool skippableBeforeNumber(char c) {
    return std::isspace(static_cast<unsigned char>(c)) || c == ':' || c == '[' || c == ',';
}

std::optional<double> numberAt(const std::string& json, size_t& pos) {
    while (pos < json.size() && skippableBeforeNumber(json[pos])) ++pos;
    if (pos >= json.size() || !startsNumber(json[pos])) return std::nullopt;
    double sign = 1.0;
    if (json[pos] == '+' || json[pos] == '-') sign = json[pos++] == '-' ? -1.0 : 1.0;
    double value = 0.0;
    bool anyDigit = false;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
        value = value * 10.0 + (json[pos++] - '0');
        anyDigit = true;
    }
    if (pos < json.size() && json[pos] == '.') {
        ++pos;
        double place = 0.1;
        while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
            value += (json[pos++] - '0') * place;
            place *= 0.1;
            anyDigit = true;
        }
    }
    if (!anyDigit) return std::nullopt;
    return sign * value;
}

std::optional<size_t> positionAfterKey(const std::string& json, const char* key) {
    const std::string quoted = '"' + std::string(key) + '"';
    const auto at = json.find(quoted);
    if (at == std::string::npos) return std::nullopt;
    return at + quoted.size();
}

std::optional<double> numberForKey(const std::string& json, const char* key) {
    const auto start = positionAfterKey(json, key);
    if (!start) return std::nullopt;
    size_t pos = *start;
    return numberAt(json, pos);
}

std::optional<std::pair<double, double>> pairForKey(const std::string& json, const char* key) {
    const auto start = positionAfterKey(json, key);
    if (!start) return std::nullopt;
    size_t pos = *start;
    const auto first = numberAt(json, pos);
    if (!first) return std::nullopt;
    const auto second = numberAt(json, pos);
    if (!second) return std::nullopt;
    return std::make_pair(*first, *second);
}

}  // namespace

std::optional<Layout> Layout::parse(const std::string& json) {
    const auto pngSize = pairForKey(json, "png_size");
    const auto x = numberForKey(json, "x");
    const auto y = numberForKey(json, "y");
    const auto w = numberForKey(json, "w");
    const auto h = numberForKey(json, "h");
    const auto cx = numberForKey(json, "cx");
    const auto cy = numberForKey(json, "cy");
    const auto r = numberForKey(json, "r");
    const auto clb = numberForKey(json, "CLB");
    const auto toga = numberForKey(json, "TOGA");
    const auto cru = numberForKey(json, "CRU");
    const auto digitCells = numberForKey(json, "digit_cells");
    const auto decimalAfter = numberForKey(json, "decimal_after");
    if (!pngSize || !x || !y || !w || !h || !cx || !cy || !r || !clb || !toga || !cru ||
        !digitCells || !decimalAfter)
        return std::nullopt;
    if (pngSize->first <= 0.0 || pngSize->second <= 0.0) return std::nullopt;

    Layout out;
    out.pngW = static_cast<float>(pngSize->first);
    out.pngH = static_cast<float>(pngSize->second);
    out.displayWindow = {static_cast<float>(*x), static_cast<float>(*y),
                         static_cast<float>(*w), static_cast<float>(*h)};
    out.knob = {static_cast<float>(*cx),  static_cast<float>(*cy),   static_cast<float>(*r),
                static_cast<float>(*clb), static_cast<float>(*toga), static_cast<float>(*cru)};
    out.digitCells = static_cast<int>(std::lround(*digitCells));
    out.decimalAfter = static_cast<int>(std::lround(*decimalAfter));
    if (out.digitCells < 1) return std::nullopt;
    return out;
}

std::optional<Layout> Layout::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) return std::nullopt;
    std::ostringstream text;
    text << file.rdbuf();
    return parse(text.str());
}

}  // namespace sfn1
