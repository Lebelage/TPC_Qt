#pragma once

#include <condition_variable>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <vector>
#include <functional>
#include "tpc/analytics/models/field_quality.hpp"

#include "tpc/analytics/models/three_dimension_models.hpp"
#include "tpc/core/definitions/analytics_definitions.hpp"
#include "tpc/system/client/client.hpp"
#include "tpc/system/models/data.hpp"
#include "tpc/utilities/event_handler.hpp"
namespace tpc::system {

using ReceivedItem = models::ReceivedItem;

struct AnalyticsImpl;
/**
 * @class TPC
 * @brief High-level facade controller managing communication with the TPC
 * hardware subsystem.
 *
 * Encapsulates client lifecycle management, asynchronous connection workflows,
 * telemetry frame acquisition, and event dispatching for upstream UI/Services.
 */
class TPC {
public:
    /**
     * @brief Factory method that instantiates and initializes a TPC controller.
     *
     * Creates the underlying client instance, verifies connection parameters,
     * and binds event handlers.
     *
     * @param[in] endpoint The OPC UA server connection URL (e.g.,
     * "opc.tcp://127.0.0.1:4840").
     * @return std::expected<std::unique_ptr<TPC>, std::string>
     *         - On success: A `std::unique_ptr<TPC>` owning the controller.
     *         - On failure: A `std::unexpected` containing the diagnostic error
     * message.
     */
    [[nodiscard]] static std::expected<std::unique_ptr<TPC>, std::string> create(std::string_view endpoint);

    /**
     * @brief Default virtual-safe destructor.
     */
    ~TPC();

    TPC(const TPC&) = delete;
    TPC& operator=(const TPC&) = delete;

    TPC(TPC&&) = delete;
    TPC& operator=(TPC&&) = delete;

public:
    /**
     * @brief Initiates an asynchronous connection routine to the target endpoint.
     */
    auto start_async() -> void;

    /**
     * @brief Halts background worker routines and disconnects from the endpoint.
     */
    auto stop_async() -> void;

    /**
     * @brief Checks whether the client communication loop is actively executing.
     * @return True if the communication pipeline is running, false otherwise.
     */
    [[nodiscard]] auto is_running() const -> bool;

    /**
     * @brief Requests the latest snapshot of sensor telemetry values.
     * @return `std::optional` containing a map of sensor names to numeric values,
     *         or `std::nullopt` if no frame is currently available.
     */
    [[nodiscard]] auto get_frame_request() -> std::optional<std::unordered_map<std::string, double>>;
    [[nodiscard]] auto get_telemetry_request() -> models::TelemetryFrame;
    void set_input_in_gauss(bool in_gauss) noexcept { input_in_gauss_ = in_gauss; }
    std::expected<void, std::string> set_reference_field(std::function<std::expected<analytics::models::FieldComponents, std::string>(std::array<double, 3>)> reference);

    /**
     * @brief Creates a deterministic test frame in gauss without reading OPC UA.
     *
     * Produces R/F components close to 0.5 G and a Z component close to 5000 G
     * for E1..E6 and W1..W6.
     */
    [[nodiscard]] static auto create_test_frame() -> std::unordered_map<std::string, double>;

    /**
     * @brief Starts polling frames or changes the interval of the running poller.
     * @param polling_interval_ms A non-zero interval between requests.
     */
    auto start_polling_async(size_t polling_interval_ms) -> void;

    auto calculate_field_async(std::vector<analytics::models::Measurement> measurements, std::array<size_t, tpc::core::definitions::DIMENSION> grid, double radius, double length,
        analytics::models::ReconstructionLimits limits = {}) -> std::expected<void, std::string>;

    [[nodiscard]] auto get_field_slice(
        analytics::models::SliceDirection direction,
        double coordinate,
        std::array<std::size_t, 2> grid,
        double radius,
        double length,
        std::stop_token stop_token = {}
    ) const -> std::expected<analytics::models::FieldSlice, std::string>;

    auto export_to_vtk(std::string_view file_path) -> std::expected<void, std::string>;

private:
    explicit TPC(std::string endpoint);

    double millivolts_to_gauss(double voltage_volts, const models::HallCalibration& calibration) noexcept;

    auto field_worker_loop(std::stop_token stop_token) -> void;

    auto polling_worker_loop(std::stop_token stop_token) -> void;

private:
    auto on_client_error(const std::string& err) -> void;
    auto on_client_info(const std::string& info) -> void;
    auto on_client_initialization_data_received(const models::DiscoveryResult& result) -> void;
    auto on_client_connection_state_changed(client::ConnectionState state) -> void;


    auto initialize_start_handlers() -> void;

public:
    /** @brief Emitted when a subsystem error occurs. */
    utilities::event_handler<std::string> error_occurred_;

    /** @brief Emitted when a subsystem warning is raised. */
    utilities::event_handler<std::string> warning_occurred_;

    /** @brief Emitted for informational diagnostic messages. */
    utilities::event_handler<std::string> info_occurred_;

    /** @brief Emitted when the client connection state transitions. */
    utilities::event_handler<client::ConnectionState> connection_state_changed_;

    /** @brief Emitted when initial sensor discovery metadata is received. */
    utilities::event_handler<models::DiscoveryResult> initialization_data_received_;

    /** @brief Emitted when the polling worker receives a frame. */
    utilities::event_handler<std::unordered_map<std::string, double>> frame_received_;
    utilities::event_handler<models::TelemetryFrame> telemetry_received_;
    utilities::event_handler<analytics::models::FieldQuality> field_quality_changed_;

    utilities::event_handler<bool> field_was_calculated_;

private:
    std::unique_ptr<struct AnalyticsImpl> impl_;
    std::unique_ptr<client::Client> client_;

    std::mutex field_worker_mutex_;
    std::condition_variable_any field_worker_cv_;
    std::optional<models::CalculationData> pending_calculation_data_;
    bool field_calculation_in_progress_{false};

    std::jthread field_worker_;

    std::mutex polling_worker_mutex_;
    std::condition_variable_any polling_worker_cv_;
    std::jthread polling_worker_;
    size_t polling_interval_ms_{0};
    size_t polling_interval_revision_{0};
    std::atomic_bool input_in_gauss_{false};
};

}  // namespace tpc::system
