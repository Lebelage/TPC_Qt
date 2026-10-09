#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <vector>

#include "models/application_settings_model.hpp"
#include "models/field_slice_model.hpp"
#include "services/scoped_subscription.hpp"
#include "tpc/utilities/event_handler.hpp"

namespace tpc_slint::services {

class EventDispatcher;
class TpcService;

struct RenderedFieldSlice {
    static constexpr std::size_t induction_band_count = 10;

    std::size_t width{};
    std::size_t height{};
    std::vector<std::uint8_t> rgba;
    std::vector<double> field;
    std::vector<std::uint8_t> valid;
    std::array<double, 2> u_range{};
    std::array<double, 2> v_range{};
    double minimum_value{};
    double maximum_value{};
    int axis{2};
    double coordinate{};

    struct Probe {
        std::array<double, 3> position_mm;
        std::array<double, 3> cartesian_field;
    };

    // Match image-fit: contain, including its centred margins. Report the
    // nearest actual grid sample rather than inventing subpixel precision.
    [[nodiscard]] std::optional<Probe> probe(
        double mouse_x, double mouse_y, double viewport_width, double viewport_height
    ) const {
        if (width < 2 || height < 2 || axis < 0 || axis > 2
            || !std::isfinite(mouse_x) || !std::isfinite(mouse_y)
            || !std::isfinite(viewport_width) || !std::isfinite(viewport_height)
            || viewport_width <= 0 || viewport_height <= 0) return std::nullopt;
        const double scale = std::min(viewport_width / width, viewport_height / height);
        const double display_width = scale * width;
        const double display_height = scale * height;
        const double x = mouse_x - (viewport_width - display_width) * .5;
        const double y = mouse_y - (viewport_height - display_height) * .5;
        if (x < 0 || y < 0 || x >= display_width || y >= display_height) return std::nullopt;
        const auto column = std::min(static_cast<std::size_t>(x / scale), width - 1);
        const auto row = std::min(static_cast<std::size_t>(y / scale), height - 1);
        const auto pixel = row * width + column;
        if (pixel >= valid.size() || !valid[pixel] || pixel * 3 + 2 >= field.size()) return std::nullopt;
        const double u = std::lerp(u_range[0], u_range[1], static_cast<double>(column) / (width - 1));
        const double v = std::lerp(v_range[1], v_range[0], static_cast<double>(row) / (height - 1));
        Probe result;
        switch (axis) {
            case 0: result.position_mm = {coordinate, u, v}; break;
            case 1: result.position_mm = {u, coordinate, v}; break;
            default: result.position_mm = {u, v, coordinate}; break;
        }
        std::copy_n(field.begin() + pixel * 3, 3, result.cartesian_field.begin());
        for (double value : result.cartesian_field) if (!std::isfinite(value)) return std::nullopt;
        return result;
    }
};

/** Requests and renders only the currently visible analytical field slice. */
class FieldSliceService final {
public:
    FieldSliceService(EventDispatcher& events, TpcService& tpc);
    ~FieldSliceService();

    FieldSliceService(const FieldSliceService&) = delete;
    FieldSliceService& operator=(const FieldSliceService&) = delete;

    tpc::utilities::event_handler<models::FieldGeometry> field_available;
    tpc::utilities::event_handler<std::shared_ptr<const RenderedFieldSlice>> slice_rendered;
    tpc::utilities::event_handler<std::string> slice_failed;

    void requestSlice(int axis, double coordinate, int viewport_width, int viewport_height);
    void dispose() noexcept;

private:
    void onSettingsChanged(const models::AppSettings& settings);
    void onFieldWasCalculated(bool success);

    EventDispatcher& events_;
    TpcService& tpc_;
    std::atomic<std::uint64_t> requested_generation_{0};
    std::mutex worker_mutex_;
    std::jthread rendering_thread_;
    std::mutex geometry_mutex_;
    models::FieldGeometry geometry_;
    ScopedSubscription<const models::AppSettings&> settings_subscription_;
    ScopedSubscription<bool> calculation_subscription_;
};

}  // namespace tpc_slint::services
