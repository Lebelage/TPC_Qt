#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <expected>
#include <shared_mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>

#include "models/application_settings_model.hpp"
#include "models/field_slice_model.hpp"
#include "models/tpc_data_model.hpp"

namespace tpc_slint::services {

class EventDispatcher;
struct TpcServiceBackend;

/**
 * Owns the TPC backend and translates its worker-thread events into
 * application-level events and calculation data.
 */
class TpcService final {
public:
    explicit TpcService(EventDispatcher& events);

    TpcService(const TpcService&) = delete;
    TpcService& operator=(const TpcService&) = delete;
    TpcService(TpcService&&) = delete;
    TpcService& operator=(TpcService&&) = delete;

    ~TpcService();

    /** Creates a backend for the endpoint and starts its connection worker. */
    [[nodiscard]] bool connectAsync(std::string endpoint);
    void disconnect();

    [[nodiscard]] std::expected<void, std::string> calculateField();
    [[nodiscard]] std::expected<models::NumericFieldSlice, std::string> calculateFieldSlice(
        int axis,
        double coordinate,
        std::array<std::size_t, 2> grid,
        std::stop_token stop_token = {}
    );
    [[nodiscard]] bool exportFieldToVtk(std::string_view file_path);

    /** Stops background work and releases all backend subscriptions. */
    void dispose() noexcept;

private:
    void subscribeToBackendEvents();
    void startPolling();
    void onSettingsChanged(const models::AppSettings& settings);
    void onFrameReceived(std::unordered_map<std::string, double> frame);
    void onFieldWasCalculated(bool is_calculated);

private:
    EventDispatcher& events_;
    mutable std::shared_mutex backend_mutex_;
    mutable std::mutex data_mutex_;
    models::TpcDataModel tpc_data_;
    std::size_t polling_interval_ms_{0};
    models::AppSettings settings_snapshot_;
    std::size_t settings_revision_{};
    std::size_t calculation_revision_{};
    bool field_snapshot_valid_{};

    std::unique_ptr<TpcServiceBackend> backend_;
};

}  // namespace tpc_slint::services
