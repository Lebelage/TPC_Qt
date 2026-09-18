#include "services/tpc_service/tpc_service.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <unordered_map>

#include "models/application_settings_model.hpp"
#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/settings_holder/settings_holder.hpp"
#include "tpc_analytics/models/three_dimension_models.hpp"
namespace tpc_qt::services {
#pragma region Constructor/Destructor
TpcService& TpcService::instance() {
    static TpcService service;
    return service;
}

TpcService::TpcService() {
    auto result = tpc::system::TPC::create("opc.tcp://127.0.0.1:1234");

    if (!result)
        return;

    tpc_ = std::move(*result);

    tpc_->connection_state_changed_.subscribe([this](tpc::system::client::ConnectionState state) {
        on_connection_state_changed(state);
    });

    tpc_->initialization_data_received_.subscribe([this](tpc::system::models::DiscoveryResult result) {
        on_client_initialization_data_received(result);
    });

    EventDispatcher::instance().settings_changed.subscribe([this](const models::AppSettings& settings) {
        on_settings_changed(settings);
    });
}

TpcService::~TpcService() {
    dispose();
}

auto TpcService::dispose() -> void {
    connection_state_changed_.dispose();
    disconnect_async();
}
#pragma endregion

#pragma region Properties
ConnectionStatus TpcService::get_connection_status() const noexcept {
    std::scoped_lock lock{mutex_};
    return connection_status_;
}

auto TpcService::get_frame_request() -> std::optional<std::unordered_map<std::string, double>> {
    auto result = tpc_->get_frame_request();
    if (!result)
        return std::nullopt;

    tpc_data_.set_received_frame(result.value());

    return result.value();
}

auto TpcService::get_initialization_data() const -> std::optional<tpc::system::models::DiscoveryResult> {
    // return tpc_data_.get_discovery_result();
}
#pragma endregion

#pragma region Public methods

void TpcService::set_connection_parameters(std::string endpoint) {
    // connection_parameters_.endpoint = endpoint;
}

bool TpcService::connect_async(std::string endpoint) {
    tpc_->start_async();
    return true;
}

void TpcService::disconnect_async() {
    if (tpc_)
        tpc_->stop_async();
}

void TpcService::calculate_field_3d() {
    if (!tpc_)
        return;

    tpc_->calculate_field_3d(create_measurments());
}

#pragma endregion

#pragma region Private methods
std::vector<tpc::analytics::models::Measurement> TpcService::create_measurments() {
    auto sensors = tpc_data_.sensors();

    std::vector<tpc::analytics::models::Measurement> measurements;

    for (const auto& sensor : sensors) {
        tpc::analytics::models::PointComponents point{
            .components = {sensor.position | std::ranges::to<std::vector<double>>()},
            .coordinate_type = tpc::analytics::models::CoordinateType::Cylindric
        };

        tpc::analytics::models::FieldComponents field{
            .components = {sensor.values | std::ranges::to<std::vector<double>>()},
            .coordinate_type = tpc::analytics::models::CoordinateType::Cylindric
        };

        measurements.push_back({.point_components = point, .field_components = field});
    }

    return measurements;
}
#pragma endregion

#pragma region Handlers
auto TpcService::on_connection_state_changed(tpc::system::client::ConnectionState state) -> void {
    connection_state_changed_.invoke(state);
}

auto TpcService::on_client_initialization_data_received(tpc::system::models::DiscoveryResult discovery_result) -> void {
    auto result = SensorName::parse_names_range(discovery_result.nodes | std::views::values);

    if (!result)
        return;

    EventDispatcher::instance().initialization_data_received.invoke(result.value());

    initialization_data_received_.invoke(discovery_result);
}

auto TpcService::on_settings_changed(const models::AppSettings& settings) -> void {
    std::vector<Sensor> sensors{};

    for (const auto& sensor : settings.sensors_info) {
        sensors.push_back({
            .name = sensor.name, .position = {sensor.x, sensor.y, sensor.z}
        });
    }

    tpc_data_.set_sensors(sensors);
}

#pragma endregion
}  // namespace tpc_qt::services
