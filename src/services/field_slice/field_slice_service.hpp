#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
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
