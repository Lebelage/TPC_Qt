#include "services/field_slice/field_slice_service.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/tpc_service/tpc_service.hpp"

namespace tpc_slint::services {
namespace {

[[nodiscard]] std::array<std::uint8_t, 3> viridis(double normalized) {
    static constexpr std::array<std::array<double, 3>, 6> colors{{
        {0.267, 0.005, 0.329}, {0.254, 0.265, 0.530}, {0.164, 0.471, 0.558},
        {0.135, 0.659, 0.518}, {0.478, 0.821, 0.318}, {0.993, 0.906, 0.144}
    }};
    normalized = std::clamp(normalized, 0.0, 1.0);
    const double scaled = normalized * static_cast<double>(colors.size() - 1);
    const auto index = std::min(static_cast<std::size_t>(scaled), colors.size() - 2);
    const double amount = scaled - static_cast<double>(index);
    const auto channel = [&](std::size_t component) {
        return static_cast<std::uint8_t>(255.0 * std::lerp(
            colors[index][component], colors[index + 1][component], amount
        ));
    };
    return {channel(0), channel(1), channel(2)};
}

[[nodiscard]] std::shared_ptr<const RenderedFieldSlice> renderSlice(
    models::NumericFieldSlice slice,
    std::stop_token stop_token
) {
    const auto width = slice.grid[0];
    const auto height = slice.grid[1];
    const std::size_t pixel_count = width * height;
    if (width < 2 || height < 2 || slice.field.size() != pixel_count * 3
        || slice.valid.size() != pixel_count) {
        return {};
    }

    auto result = std::make_shared<RenderedFieldSlice>();
    result->width = width;
    result->height = height;
    result->rgba.assign(pixel_count * 4, 0);
    result->field = std::move(slice.field);
    result->valid = std::move(slice.valid);
    result->u_range = slice.horizontal_bounds;
    result->v_range = slice.vertical_bounds;
    result->minimum_value = std::numeric_limits<double>::infinity();
    result->maximum_value = -std::numeric_limits<double>::infinity();

    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
        if (!result->valid[pixel]) {
            continue;
        }
        const std::size_t offset = pixel * 3;
        const double value = std::hypot(
            result->field[offset], result->field[offset + 1], result->field[offset + 2]
        );
        result->minimum_value = std::min(result->minimum_value, value);
        result->maximum_value = std::max(result->maximum_value, value);
    }
    if (!std::isfinite(result->minimum_value)) {
        return {};
    }

    const double value_span = result->maximum_value - result->minimum_value;
    std::vector<std::uint8_t> induction_bands(pixel_count);
    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
        if (stop_token.stop_requested()) {
            return {};
        }
        if (!result->valid[pixel]) {
            continue;
        }
        const std::size_t field_offset = pixel * 3;
        const double value = std::hypot(
            result->field[field_offset], result->field[field_offset + 1], result->field[field_offset + 2]
        );
        const double normalized = value_span > 0.0
            ? (value - result->minimum_value) / value_span
            : 0.5;
        const auto color = viridis(normalized);
        const std::size_t rgba_offset = pixel * 4;
        result->rgba[rgba_offset] = color[0];
        result->rgba[rgba_offset + 1] = color[1];
        result->rgba[rgba_offset + 2] = color[2];
        result->rgba[rgba_offset + 3] = 255;
        induction_bands[pixel] = static_cast<std::uint8_t>(std::min(
            static_cast<std::size_t>(normalized * RenderedFieldSlice::induction_band_count),
            RenderedFieldSlice::induction_band_count - 1
        ));
    }

    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const std::size_t pixel = y * width + x;
            if (!result->valid[pixel]) {
                continue;
            }
            const bool horizontal = x > 0 && result->valid[pixel - 1]
                && induction_bands[pixel] != induction_bands[pixel - 1];
            const bool vertical = y > 0 && result->valid[pixel - width]
                && induction_bands[pixel] != induction_bands[pixel - width];
            if (horizontal || vertical) {
                const std::size_t offset = pixel * 4;
                result->rgba[offset] = static_cast<std::uint8_t>(result->rgba[offset] * 2 / 5);
                result->rgba[offset + 1] = static_cast<std::uint8_t>(result->rgba[offset + 1] * 2 / 5);
                result->rgba[offset + 2] = static_cast<std::uint8_t>(result->rgba[offset + 2] * 2 / 5);
            }
        }
    }
    return result;
}

}  // namespace

FieldSliceService::FieldSliceService(EventDispatcher& events, TpcService& tpc) : events_(events), tpc_(tpc) {
    settings_subscription_.subscribe(
        events.settings_changed,
        [this](const models::AppSettings& settings) { onSettingsChanged(settings); }
    );
    calculation_subscription_.subscribe(
        events.field_was_calculated_,
        [this](bool success) { onFieldWasCalculated(success); }
    );
}

FieldSliceService::~FieldSliceService() {
    dispose();
}

void FieldSliceService::dispose() noexcept {
    requested_generation_.fetch_add(1);
    std::scoped_lock lock{worker_mutex_};
    if (rendering_thread_.joinable()) {
        rendering_thread_.request_stop();
        rendering_thread_.join();
    }
}

void FieldSliceService::requestSlice(int axis, double coordinate, int viewport_width, int viewport_height) {
    viewport_width = std::clamp(viewport_width, 128, 1024);
    viewport_height = std::clamp(viewport_height, 128, 1024);
    models::FieldGeometry geometry;
    {
        std::scoped_lock lock{geometry_mutex_};
        geometry = geometry_;
    }
    const double u_span = axis == 2
        ? 2.0 * geometry.radius
        : 2.0 * std::sqrt(std::max(0.0, geometry.radius * geometry.radius - coordinate * coordinate));
    const double v_span = axis == 2 ? 2.0 * geometry.radius : geometry.length;
    int grid_width = viewport_width;
    int grid_height = viewport_height;
    if (u_span > 0.0 && v_span > 0.0) {
        const double scale = std::min(
            static_cast<double>(viewport_width) / u_span,
            static_cast<double>(viewport_height) / v_span
        );
        grid_width = std::max(2, static_cast<int>(std::round(u_span * scale)));
        grid_height = std::max(2, static_cast<int>(std::round(v_span * scale)));
    } else {
        grid_width = 2;
    }

    const std::uint64_t generation = requested_generation_.fetch_add(1) + 1;
    std::scoped_lock lock{worker_mutex_};
    if (rendering_thread_.joinable()) {
        rendering_thread_.request_stop();
        rendering_thread_.join();
    }
    rendering_thread_ = std::jthread([
        this, axis, coordinate, grid_width, grid_height, generation
    ](std::stop_token stop_token) {
        auto numeric_slice = tpc_.calculateFieldSlice(
            axis,
            coordinate,
            {static_cast<std::size_t>(grid_width), static_cast<std::size_t>(grid_height)},
            stop_token
        );
        if (stop_token.stop_requested() || requested_generation_.load() != generation) {
            return;
        }
        if (!numeric_slice) {
            slice_failed.invoke(numeric_slice.error());
            return;
        }
        auto rendered = renderSlice(std::move(*numeric_slice), stop_token);
        if (rendered && !stop_token.stop_requested() && requested_generation_.load() == generation) {
            events_.diagnostic.invoke(LogLevel::Info, "slice", std::format(
                "Slice rendered: axis={}, coordinate={} cm, grid={}x{}", axis, coordinate, grid_width, grid_height));
            slice_rendered.invoke(std::move(rendered));
        } else if (!stop_token.stop_requested() && requested_generation_.load() == generation) {
            slice_failed.invoke("Slice has no finite field values");
        }
    });
}

void FieldSliceService::onSettingsChanged(const models::AppSettings& settings) {
    std::scoped_lock lock{geometry_mutex_};
    geometry_ = {
        .radius = settings.geometry.radius,
        .length = settings.geometry.length,
        .grid = settings.grid
    };
}

void FieldSliceService::onFieldWasCalculated(bool success) {
    if (!success) {
        return;
    }
    models::FieldGeometry geometry;
    {
        std::scoped_lock lock{geometry_mutex_};
        geometry = geometry_;
    }
    field_available.invoke(geometry);
}

}  // namespace tpc_slint::services
