#pragma once

#include <optional>
#include <string>
#include <vector>

namespace sfn1 {

/// One AFM N1 schedule: a grid of N1 % over OAT/RAT (°C, rows) and pressure
/// altitude (ft, columns), parsed from the CSV format described in
/// docs/CONTRACTS.md. Empty cells mean "no charted data".
class N1Table {
public:
    /// Parses a CSV grid (header row = altitudes ft ascending, first column =
    /// temperatures °C ascending, cell (0,0) ignored). Returns nullopt on any
    /// malformed input: ragged rows, non-numeric axis values or cells,
    /// non-ascending axes.
    static std::optional<N1Table> loadFromCsv(const std::string& csvText);

    /// Bilinear interpolation. Nullopt when there is no charted data: either
    /// an input lies outside the grid axes, or one of the surrounding cells is
    /// empty. The device shows dashes in both cases.
    std::optional<double> lookup(double oatC, double paFt) const;

private:
    N1Table() = default;

    std::vector<double> oatsC_;
    std::vector<double> altsFt_;
    std::vector<std::vector<std::optional<double>>> cells_;
};

}  // namespace sfn1
