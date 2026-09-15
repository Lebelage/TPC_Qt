#include "services/settings_holder/settings_holder.hpp"

#include <algorithm>
#include <expected>
#include <iostream>

#include "nlohmann/json.hpp"
#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/file_worker/file_worker.hpp"

namespace tpc_qt::services {
#pragma region Constructor/Destructor
SettingsHolderService& SettingsHolderService::instance() {
    static SettingsHolderService service;
    return service;
}

SettingsHolderService::SettingsHolderService() {
    current_settings_ = initialize_by_defaults();
}

SettingsHolderService::~SettingsHolderService() {}

#pragma endregion

#pragma region Public methods

void SettingsHolderService::apply_settings() {
    nlohmann::json j = current_settings_;

    FileWorker::instance().write_settings(j.dump(4));

    EventDispatcher::instance().settings_applied.invoke(current_settings_);
}

std::expected<void, std::string> SettingsHolderService::load_settings() {
    try {
        auto result = FileWorker::instance().load_settings();

        if (!result) {
            current_settings_ = initialize_by_defaults();

            EventDispatcher::instance().settings_loaded.invoke(current_settings_);

            return std::unexpected(result.error());
        }

        models::AppSettings deserialized = nlohmann::json::parse(result.value()).get<models::AppSettings>();

        current_settings_ = deserialized;

        EventDispatcher::instance().settings_loaded.invoke(current_settings_);

        return {};
    } catch (...) {
        current_settings_ = initialize_by_defaults();
        return {};
    }
}

void SettingsHolderService::set_connection_parameters(ConnectionParameters connection_parameters) {
    current_settings_.connection = connection_parameters;
}

void SettingsHolderService::set_geometry_parameters(TpcGeometryParams geometry_parameters) {
    current_settings_.geometry = geometry_parameters;
}

void SettingsHolderService::set_sensors_parameters(std::vector<SensorInfo> sensors_parameters) {
    if (sensors_parameters.empty())
        current_settings_.sensors_info = sensors_parameters;

    current_settings_.sensors_info = validate_sensors_parameters(sensors_parameters);
}

ConnectionParameters SettingsHolderService::get_connection_parameters() {
    return current_settings_.connection;
}

TpcGeometryParams SettingsHolderService::get_geometry_parameters() {
    return current_settings_.geometry;
}

std::vector<SensorInfo> SettingsHolderService::get_sensors_parameters() {
    return current_settings_.sensors_info;
}

const models::AppSettings& SettingsHolderService::get_current_settings() const noexcept {
    return current_settings_;
}

#pragma endregion

#pragma region Private methods
models::AppSettings SettingsHolderService::initialize_by_defaults() {
    return models::AppSettings{
        {                       228, 1337},
        {"opc.tcp://127.0.0.1:1234", 1000}
    };
}

std::vector<SensorInfo> SettingsHolderService::validate_sensors_parameters(std::vector<SensorInfo> sensors_parameters) {
    return sensors_parameters;
}
#pragma endregion

#pragma region Private methods
void SettingsHolderService::initialization_data_received(tpc::system::models::DiscoveryResult discovery_result) {
    auto discovery_sensors_info =
        discovery_result.nodes | std::views::values | std::ranges::to<std::vector<SensorInfo>>();

    if (current_settings_.sensors_info.empty()) {
        set_sensors_parameters(discovery_sensors_info);

        return;
    }

    for (const auto& sensor : current_settings_.sensors_info) {
        auto it = std::find_if(
            discovery_sensors_info.begin(),
            discovery_sensors_info.end(),
            [&sensor](const SensorInfo& discovery_sensor) {
                return discovery_sensor.id == sensor.id;
            }
        );

        
    }
}
#pragma endregion

}  // namespace tpc_qt::services
