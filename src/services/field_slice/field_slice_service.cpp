#include "services/field_slice/field_slice_service.hpp"

#include <cmath>

#include <algorithm>
#include <utility>

#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/tpc_service/tpc_service.hpp"

namespace tpc_slint::services {
FieldSliceService::FieldSliceService(EventDispatcher& events, TpcService& tpc) : events_(events), tpc_(tpc) {
    settings_subscription_.subscribe(events.settings_changed, [this](const models::AppSettings& settings) {
        onSettingsChanged(settings);
    });
    calculation_subscription_.subscribe(events.field_was_calculated_, [this](bool success) {
        onFieldWasCalculated(success);
    });
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
    // Keep the same physical frame and sampling density while the slice moves.
    const double u_span = 2.0 * geometry.radius;
    const double v_span = axis == 2 ? 2.0 * geometry.radius : geometry.length;
    int grid_width = viewport_width;
    int grid_height = viewport_height;
    if (u_span > 0.0 && v_span > 0.0) {
        const double scale = std::min(
            static_cast<double>(viewport_width) / (axis == 2 ? u_span : v_span),
            static_cast<double>(viewport_height) / (axis == 2 ? v_span : u_span)
        );
        grid_width = std::max(2, static_cast<int>(std::round(u_span * scale)));
        grid_height = std::max(2, static_cast<int>(std::round(v_span * scale)));
    } else {
        grid_width = 2;
        grid_height = axis == 2 ? viewport_height : viewport_width;
    }
    // At a tangent slice, the remaining valid samples lie on transverse zero.
    // Include that node so both cylinder boundaries remain inspectable.
    if (axis != 2)
        grid_width = std::clamp(grid_width | 1, 3, 1023);

    const std::uint64_t generation = requested_generation_.fetch_add(1) + 1;
    std::scoped_lock lock{worker_mutex_};
    if (rendering_thread_.joinable()) {
        rendering_thread_.request_stop();
        rendering_thread_.join();
    }
    rendering_thread_ = std::jthread([this,
                                      axis,
                                      coordinate,
                                      grid_width,
                                      grid_height,
                                      generation,
                                      radius = geometry.radius](std::stop_token stop_token) {
        auto numeric_slice = tpc_.calculateFieldSlice(
            axis, coordinate, {static_cast<std::size_t>(grid_width), static_cast<std::size_t>(grid_height)}, stop_token
        );
        if (stop_token.stop_requested() || requested_generation_.load() != generation) {
            return;
        }
        if (!numeric_slice) {
            slice_failed.invoke(numeric_slice.error());
            return;
        }
        auto rendered = renderFieldSlice(std::move(*numeric_slice), axis, coordinate, radius, stop_token);
        if (rendered && !stop_token.stop_requested() && requested_generation_.load() == generation) {
            events_.diagnostic.invoke(
                LogLevel::Info,
                "slice",
                std::format(
                    "Slice rendered: axis={}, coordinate={} mm, grid={}x{}", axis, coordinate, grid_width, grid_height
                )
            );
            slice_rendered.invoke(std::move(rendered));
        } else if (!stop_token.stop_requested() && requested_generation_.load() == generation) {
            slice_failed.invoke("Slice has no finite field values");
        }
    });
}

void FieldSliceService::onSettingsChanged(const models::AppSettings& settings) {
    std::scoped_lock lock{geometry_mutex_};
    if (settings.runtime_revision && settings.runtime_revision < settings_revision_) return;
    settings_revision_ = settings.runtime_revision;
    geometry_ = {.radius = settings.geometry.radius, .length = settings.geometry.length, .grid = settings.grid};
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
