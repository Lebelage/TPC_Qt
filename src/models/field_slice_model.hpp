#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tpc_slint::models {

struct FieldGeometry {
    double radius{};
    double length{};
    std::array<std::size_t, 3> grid{};
};

/** One regular slice evaluated directly by TPC_API. */
struct NumericFieldSlice {
    std::array<std::size_t, 2> grid{};
    std::array<double, 2> horizontal_bounds{};
    std::array<double, 2> vertical_bounds{};
    std::vector<double> field;
    std::vector<std::uint8_t> valid;
};

}  // namespace tpc_slint::models
