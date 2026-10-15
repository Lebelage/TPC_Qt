#include <cmath>

#include <algorithm>
#include <limits>
#include <utility>

#include "services/field_slice/rendered_field_slice.hpp"

namespace tpc_slint::services {
namespace {
constexpr int samples_per_axis = 4;

std::array<double, 3> viridis(double normalized) {
    static constexpr std::array<std::array<double, 3>, 6> colors{
        {{0.267, 0.005, 0.329},
         {0.254, 0.265, 0.530},
         {0.164, 0.471, 0.558},
         {0.135, 0.659, 0.518},
         {0.478, 0.821, 0.318},
         {0.993, 0.906, 0.144}}
    };
    const double scaled = std::clamp(normalized, 0.0, 1.0) * (colors.size() - 1);
    const auto index = std::min(static_cast<std::size_t>(scaled), colors.size() - 2);
    std::array<double, 3> color;
    for (std::size_t component = 0; component < color.size(); ++component) {
        color[component] = 255 * std::lerp(colors[index][component], colors[index + 1][component], scaled - index);
    }
    return color;
}

struct VisualSample {
    double value{};
    double gradient_x{};
    double gradient_y{};
};

// Interpolate the colour scalar, excluding masked nodes. Quotient derivatives
// keep contour thickness consistent near the cylinder boundary as well.
std::optional<VisualSample> interpolate(
    const std::vector<double>& values, std::size_t width, std::size_t height, double x, double y
) {
    const auto column = std::min(static_cast<std::size_t>(std::max(0.0, x)), width - 2);
    const auto row = std::min(static_cast<std::size_t>(std::max(0.0, y)), height - 2);
    const double fx = std::clamp(x - column, 0.0, 1.0);
    const double fy = std::clamp(y - row, 0.0, 1.0);
    const std::array<std::size_t, 4> nodes{
        row * width + column, row * width + column + 1, (row + 1) * width + column, (row + 1) * width + column + 1
    };
    const std::array<double, 4> weights{(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
    const std::array<double, 4> dx{-(1 - fy), 1 - fy, -fy, fy};
    const std::array<double, 4> dy{-(1 - fx), -fx, 1 - fx, fx};
    double weight = 0, value = 0, weight_x = 0, weight_y = 0, value_x = 0, value_y = 0;
    for (std::size_t corner = 0; corner < nodes.size(); ++corner) {
        const double scalar = values[nodes[corner]];
        if (!std::isfinite(scalar))
            continue;
        weight += weights[corner];
        value += weights[corner] * scalar;
        weight_x += dx[corner];
        weight_y += dy[corner];
        value_x += dx[corner] * scalar;
        value_y += dy[corner] * scalar;
    }
    if (weight <= 1e-12)
        return std::nullopt;
    return VisualSample{
        value / weight,
        (value_x * weight - value * weight_x) / (weight * weight),
        (value_y * weight - value * weight_y) / (weight * weight)
    };
}
}  // namespace

std::shared_ptr<const RenderedFieldSlice> renderFieldSlice(
    models::NumericFieldSlice slice, int axis, double coordinate, double radius, std::stop_token stop_token
) {
    const auto width = slice.grid[0];
    const auto height = slice.grid[1];
    if (stop_token.stop_requested() || width < 2 || height < 2 ||
        width > std::numeric_limits<std::size_t>::max() / height / 4)
        return {};
    const std::size_t pixel_count = width * height;
    if (axis < 0 || axis > 2 || !std::isfinite(radius) || radius <= 0 || slice.field.size() != pixel_count * 3 ||
        slice.valid.size() != pixel_count)
        return {};
    auto result = std::make_shared<RenderedFieldSlice>();
    result->axis = axis;
    result->coordinate = coordinate;
    result->width = width;
    result->height = height;
    result->rgba.assign(pixel_count * 4, 0);
    result->field = std::move(slice.field);
    result->valid = std::move(slice.valid);
    result->u_range = slice.horizontal_bounds;
    result->v_range = slice.vertical_bounds;
    result->minimum_value = std::numeric_limits<double>::infinity();
    result->maximum_value = -std::numeric_limits<double>::infinity();
    std::vector<double> values(pixel_count, std::numeric_limits<double>::quiet_NaN());
    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
        if (stop_token.stop_requested())
            return {};
        if (!result->valid[pixel])
            continue;
        const auto offset = pixel * 3;
        const double magnitude =
            std::hypot(result->field[offset], result->field[offset + 1], result->field[offset + 2]);
        if (!std::isfinite(magnitude))
            continue;
        values[pixel] = magnitude;
        result->minimum_value = std::min(result->minimum_value, magnitude);
        result->maximum_value = std::max(result->maximum_value, magnitude);
    }
    if (!std::isfinite(result->minimum_value))
        return {};
    const double span = result->maximum_value - result->minimum_value;
    for (auto& value : values) {
        if (std::isfinite(value))
            value = span > 0 ? (value - result->minimum_value) / span : .5;
    }

    for (std::size_t row = 0; row < height; ++row) {
        for (std::size_t column = 0; column < width; ++column) {
            if (stop_token.stop_requested())
                return {};
            std::array<double, 3> color{};
            int covered = 0;
            // 4x4 coverage sampling antialiases both the physical outline and
            // 1px isolines, without blurring the whole image or changing data.
            for (int sy = 0; sy < samples_per_axis; ++sy) {
                const double y = row + (sy + .5) / samples_per_axis - .5;
                for (int sx = 0; sx < samples_per_axis; ++sx) {
                    const double x = column + (sx + .5) / samples_per_axis - .5;
                    if (x < 0 || y < 0 || x > width - 1 || y > height - 1)
                        continue;
                    {
                        const double u = std::lerp(result->u_range[0], result->u_range[1], x / (width - 1));
                        const double v = std::lerp(result->v_range[1], result->v_range[0], y / (height - 1));
                        const double radial = axis == 2 ? std::hypot(u, v) : std::hypot(coordinate, u);
                        if (radial > radius)
                            continue;
                    }
                    const auto sample = interpolate(values, width, height, x, y);
                    if (!sample)
                        continue;
                    auto sample_color = viridis(sample->value);
                    const double band = std::round(sample->value * RenderedFieldSlice::induction_band_count);
                    const double gradient = std::hypot(sample->gradient_x, sample->gradient_y);
                    if (band > 0 && band < RenderedFieldSlice::induction_band_count && gradient > 1e-12 &&
                        std::abs(sample->value - band / RenderedFieldSlice::induction_band_count) < .5 * gradient) {
                        for (auto& channel : sample_color)
                            channel *= .4;
                    }
                    for (std::size_t channel = 0; channel < color.size(); ++channel)
                        color[channel] += sample_color[channel];
                    ++covered;
                }
            }
            if (!covered)
                continue;
            // Rotate only the visual buffer; retain native numeric samples for
            // field inspection and exports. Longitudinal slices lie along +Z.
            const auto display_pixel = axis == 2 ? row * width + column : column * height + height - 1 - row;
            const auto offset = display_pixel * 4;
            // Straight alpha: average only covered colours, so transparent
            // samples cannot introduce a black halo during image scaling.
            for (std::size_t channel = 0; channel < color.size(); ++channel) {
                result->rgba[offset + channel] = static_cast<std::uint8_t>(std::round(color[channel] / covered));
            }
            result->rgba[offset + 3] =
                static_cast<std::uint8_t>(std::round(255.0 * covered / (samples_per_axis * samples_per_axis)));
        }
    }
    return result;
}
}  // namespace tpc_slint::services
