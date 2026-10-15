#include "viewmodels/field_map_overlay.hpp"

#include <cmath>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>

#include "models/tpc_data_model.hpp"
#include "services/field_slice/rendered_field_slice.hpp"

namespace tpc_slint::viewmodels {

namespace {
constexpr int color_intervals = 5;
constexpr int minor_intervals = 5;
constexpr int max_axis_ticks = 512;
constexpr double major_tick_spacing_px = 74;
constexpr double slice_tolerance_mm = 1e-3;
constexpr double projection_tolerance = 1e-6;

double imageCoordinate(double value, const std::array<double, 2>& range, std::size_t pixels) {
    const double fraction = range[1] > range[0] ? (value - range[0]) / (range[1] - range[0]) : .5;
    return pixels > 1 ? (.5 + fraction * (pixels - 1)) / pixels : .5;
}

std::vector<FieldTick> axisTicks(
    const std::array<double, 2>& range,
    std::size_t pixels,
    double start,
    double size,
    double viewport_size,
    bool inverted,
    double major_step
) {
    const double minor_step = major_step / minor_intervals;
    const auto world = [&](double screen) {
        double normalized = (screen - start) / size;
        if (inverted)
            normalized = 1 - normalized;
        return range[0] + (normalized * pixels - .5) / (pixels - 1) * (range[1] - range[0]);
    };
    const double first = std::ceil(std::min(world(0), world(viewport_size)) / minor_step);
    const double last = std::floor(std::max(world(0), world(viewport_size)) / minor_step);
    std::vector<FieldTick> ticks;
    for (int i = 0; i < max_axis_ticks && first + i <= last; ++i) {
        const double index = first + i;
        const bool zero = std::abs(index) < .5;
        const bool major = std::abs(std::remainder(index, minor_intervals)) < .5;
        const double value = zero ? 0 : index * minor_step;
        const double position = imageCoordinate(value, range, pixels);
        ticks.push_back({inverted ? 1 - position : position, major ? std::format("{:g}", value) : "", zero, major});
    }
    return ticks;
}
}  // namespace

std::vector<FieldTick> fieldColorTicks(double minimum, double maximum) {
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || maximum < minimum)
        return {};
    if (minimum == maximum)
        return {
            {.5, std::format("{:.8g}", minimum), minimum == 0}
        };
    const double span = maximum - minimum;
    if (!std::isfinite(span))
        return {};
    const double magnitude = std::max(std::abs(minimum), std::abs(maximum));
    const int precision = static_cast<int>(std::clamp(
        2 + std::ceil(std::log10(magnitude) - std::log10(span) + std::log10(double(color_intervals))), 4., 15.
    ));
    std::vector<FieldTick> ticks;
    ticks.reserve(color_intervals + 1);
    for (int index = 0; index <= color_intervals; ++index) {
        const double fraction = double(index) / color_intervals;
        const double value = std::lerp(minimum, maximum, fraction);
        ticks.push_back({fraction, std::format("{:.{}g}", value, precision), value == 0});
    }
    return ticks;
}

FieldGrid fieldGrid(const services::RenderedFieldSlice& slice, const FieldViewport& view) {
    FieldGrid grid;
    const std::array dimensions{view.width, view.height, view.map_width, view.map_height};
    if (!std::ranges::all_of(
            dimensions,
            [](double value) {
                return std::isfinite(value) && value > 0;
            }
        ) ||
        !std::isfinite(view.map_x) || !std::isfinite(view.map_y) || slice.displayWidth() < 2 ||
        slice.displayHeight() < 2)
        return grid;
    const auto& u = slice.horizontalRange();
    const auto& v = slice.verticalRange();
    const double u_span = u[1] - u[0], v_span = v[1] - v[0];
    if (!std::isfinite(u_span) || !std::isfinite(v_span) || u_span <= 0 || v_span <= 0)
        return grid;
    const double u_per_pixel = u_span * slice.displayWidth() / (slice.displayWidth() - 1) / view.map_width;
    const double v_per_pixel = v_span * slice.displayHeight() / (slice.displayHeight() - 1) / view.map_height;
    const double target = major_tick_spacing_px * std::max(u_per_pixel, v_per_pixel);
    const double power = std::pow(10., std::floor(std::log10(target)));
    const double fraction = target / power;
    grid.major_step = (fraction <= 1 ? 1 : fraction <= 2 ? 2 : fraction <= 5 ? 5 : 10) * power;
    if (!std::isfinite(grid.major_step) || grid.major_step <= 0)
        return {};
    grid.horizontal =
        axisTicks(u, slice.displayWidth(), view.map_x, view.map_width, view.width, false, grid.major_step);
    grid.vertical = axisTicks(
        v, slice.displayHeight(), view.map_y, view.map_height, view.height, slice.verticalInverted(), grid.major_step
    );
    return grid;
}

std::vector<FieldSensorMarker> fieldSensorMarkers(
    const services::RenderedFieldSlice& slice, std::span<const models::Sensor> sensors
) {
    std::vector<FieldSensorMarker> markers;
    if (slice.axis < 0 || slice.axis > 2 || slice.width < 2 || slice.height < 2)
        return markers;
    const int horizontal = slice.horizontalAxis();
    const int vertical = slice.verticalAxis();
    for (const auto& sensor : sensors) {
        if (!std::ranges::all_of(sensor.position, [](double p) {
                return std::isfinite(p);
            }))
            continue;
        const double u = sensor.position[horizontal];
        const double v = sensor.position[vertical];
        if (u < slice.horizontalRange()[0] || u > slice.horizontalRange()[1] || v < slice.verticalRange()[0] ||
            v > slice.verticalRange()[1])
            continue;
        const double x = imageCoordinate(u, slice.horizontalRange(), slice.displayWidth());
        const double vertical_position = imageCoordinate(v, slice.verticalRange(), slice.displayHeight());
        const double y = slice.verticalInverted() ? 1 - vertical_position : vertical_position;
        const bool in_slice = std::abs(sensor.position[slice.axis] - slice.coordinate) <= slice_tolerance_mm;
        const auto label = sensor.name.toString() + (in_slice ? "*" : "");
        auto same = std::ranges::find_if(markers, [x, y](const auto& marker) {
            return std::abs(marker.u - x) < projection_tolerance && std::abs(marker.v - y) < projection_tolerance;
        });
        if (same == markers.end())
            markers.push_back({x, y, label, in_slice});
        else {
            same->label += " / " + label;
            same->in_slice |= in_slice;
        }
    }
    return markers;
}

}  // namespace tpc_slint::viewmodels
