#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "models/application_settings_model.hpp"
#include "models/tpc_data_model.hpp"
#include "tpc_analytics/models/three_dimension_models.hpp"
#include "tpc_system/client/client.hpp"
#include "tpc_system/models/data.hpp"
#include "tpc_system/tpc.hpp"
#include "utilities/event_handler.hpp"
namespace tpc_qt::models{
    struct AppSettings;
}

namespace tpc_qt::services {
enum class ConnectionStatus {
    Connected,
    Inactive,
    Disconnected,
};

class TpcService {
public:
    static TpcService& instance();

    TpcService(const TpcService&) = delete;

    TpcService& operator=(const TpcService&) = delete;

    TpcService(TpcService&&) = delete;

    TpcService& operator=(TpcService&&) = delete;

    ~TpcService();

private:
    TpcService();

public:
    [[nodiscard]]
    ConnectionStatus get_connection_status() const noexcept;

    [[nodiscard]] auto get_frame_request() -> std::optional<std::unordered_map<std::string, double>>;

    [[nodiscard]] auto get_initialization_data() const -> std::optional<tpc::system::models::DiscoveryResult>;

    // [[nodiscard]]
    // ConnectionStatus get_sensors_name() const noexcept;

    void set_connection_parameters(std::string endpoint);

    bool connect_async(std::string endpoint);

    void disconnect_async();

    void calculate_field_3d();

public:
    auto dispose() -> void;

private:
    std::vector<tpc::analytics::models::Measurement> create_measurments();

    

private:
    auto on_connection_state_changed(tpc::system::client::ConnectionState) -> void;

    auto on_client_initialization_data_received(tpc::system::models::DiscoveryResult) -> void;

    auto on_settings_changed(const models::AppSettings&) -> void;

public:
    tpc::utilities::event_handler<tpc::system::client::ConnectionState> connection_state_changed_;
    tpc::utilities::event_handler<tpc::system::models::DiscoveryResult> initialization_data_received_;

private:
    mutable std::mutex mutex_;

    models::TpcDataModel tpc_data_{};

    std::unique_ptr<tpc::system::TPC> tpc_;

    std::vector<std::uint8_t> handlers_ids_;

    ConnectionStatus connection_status_{ConnectionStatus::Disconnected};
};
}  // namespace tpc_qt::services
