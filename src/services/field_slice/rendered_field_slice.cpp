#include "services/field_slice/rendered_field_slice.hpp"

#include <cmath>

#include <algorithm>

namespace tpc_slint::services {

std::optional<RenderedFieldSlice::Probe> RenderedFieldSlice::probe(
    double mouse_x, double mouse_y, double viewport_width, double viewport_height
) const {
    if (width < 2 || height < 2 || axis < 0 || axis > 2 || !std::isfinite(mouse_x) || !std::isfinite(mouse_y) ||
        !std::isfinite(viewport_width) || !std::isfinite(viewport_height) || viewport_width <= 0 ||
        viewport_height <= 0)
        return std::nullopt;
    const double scale = std::min(viewport_width / displayWidth(), viewport_height / displayHeight());
    const double display_width = scale * displayWidth();
    const double display_height = scale * displayHeight();
    const double x = mouse_x - (viewport_width - display_width) * .5;
    const double y = mouse_y - (viewport_height - display_height) * .5;
    if (x < 0 || y < 0 || x >= display_width || y >= display_height)
        return std::nullopt;
    const auto display_column = std::min(static_cast<std::size_t>(x / scale), displayWidth() - 1);
    const auto display_row = std::min(static_cast<std::size_t>(y / scale), displayHeight() - 1);
    // X/Y images rotate clockwise: +Z points right, +Y/+X points down.
    const auto column = axis == 2 ? display_column : display_row;
    const auto row = axis == 2 ? display_row : height - 1 - display_column;
    const auto pixel = row * width + column;
    if (pixel >= valid.size() || !valid[pixel] || pixel * 3 + 2 >= field.size())
        return std::nullopt;
    const double u = std::lerp(u_range[0], u_range[1], static_cast<double>(column) / (width - 1));
    const double v = std::lerp(v_range[1], v_range[0], static_cast<double>(row) / (height - 1));
    Probe result;
    switch (axis) {
        case 0:
            result.position_mm = {coordinate, u, v};
            break;
        case 1:
            result.position_mm = {u, coordinate, v};
            break;
        default:
            result.position_mm = {u, v, coordinate};
            break;
    }
    std::copy_n(field.begin() + pixel * 3, 3, result.cartesian_field.begin());
    for (double value : result.cartesian_field)
        if (!std::isfinite(value))
            return std::nullopt;
    return result;
}

}  // namespace tpc_slint::services
