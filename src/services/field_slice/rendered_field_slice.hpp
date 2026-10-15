#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

#include "models/field_slice_model.hpp"

namespace tpc_slint::services {

struct RenderedFieldSlice {
    static constexpr std::size_t induction_band_count = 10;

    // Numeric grid dimensions. The displayed image is rotated for X/Y slices.
    std::size_t width{};
    std::size_t height{};
    // Rows use displayWidth(), while field/valid retain the numeric grid order.
    std::vector<std::uint8_t> rgba;
    std::vector<double> field;
    std::vector<std::uint8_t> valid;
    std::array<double, 2> u_range{};
    std::array<double, 2> v_range{};
    double minimum_value{};
    double maximum_value{};
    int axis{2};
    double coordinate{};

    [[nodiscard]] std::size_t displayWidth() const noexcept {
        return axis == 2 ? width : height;
    }
    [[nodiscard]] std::size_t displayHeight() const noexcept {
        return axis == 2 ? height : width;
    }
    [[nodiscard]] int horizontalAxis() const noexcept {
        return axis == 2 ? 0 : 2;
    }
    [[nodiscard]] int verticalAxis() const noexcept {
        return axis == 1 ? 0 : 1;
    }
    [[nodiscard]] const std::array<double, 2>& horizontalRange() const noexcept {
        return axis == 2 ? u_range : v_range;
    }
    [[nodiscard]] const std::array<double, 2>& verticalRange() const noexcept {
        return axis == 2 ? v_range : u_range;
    }
    [[nodiscard]] bool verticalInverted() const noexcept {
        return axis == 2;
    }

    struct Probe {
        std::array<double, 3> position_mm;
        std::array<double, 3> cartesian_field;
    };

    // Match image-fit: contain, including its centred margins. Report the
    // nearest actual grid sample rather than inventing subpixel precision.
    [[nodiscard]] std::optional<Probe> probe(
        double mouse_x, double mouse_y, double viewport_width, double viewport_height
    ) const;
};

// Supersample only the visual layer; field values and probe validity stay intact.
[[nodiscard]] std::shared_ptr<const RenderedFieldSlice> renderFieldSlice(
    models::NumericFieldSlice slice, int axis, double coordinate, double radius, std::stop_token stop_token = {}
);

}  // namespace tpc_slint::services
