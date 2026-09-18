#include "services/settings_holder/settings_holder.hpp"

#include <algorithm>
#include <charconv>
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
    EventDispatcher::instance().initialization_data_received.subscribe(
        [this](std::vector<SensorName> discovery_sensors_names) {
            initialization_data_received(discovery_sensors_names);
        }
    );

    load_settings();
}

SettingsHolderService::~SettingsHolderService() {}

#pragma endregion

#pragma region Public methods

void SettingsHolderService::apply_settings() {
    nlohmann::json j = current_settings_;

    FileWorker::instance().write_settings(j.dump(4));

    EventDispatcher::instance().settings_changed.invoke(current_settings_);
}

std::expected<void, std::string> SettingsHolderService::load_settings() {
    try {
        auto result = FileWorker::instance().load_settings();

        if (!result) {
            current_settings_ = initialize_by_defaults();

            apply_settings();

            return std::unexpected(result.error());
        }

        models::AppSettings deserialized = nlohmann::json::parse(result.value()).get<models::AppSettings>();

        validate_and_confirm_settings(deserialized);

        return {};
    } catch (...) {
        return {};
    }
}

void SettingsHolderService::set_connection_parameters(ConnectionParameters connection_parameters) {
    current_settings_.connection.endpoint =
        connection_parameters.endpoint.empty() ? "opc.tcp://127.0.0.1:1234" : connection_parameters.endpoint;
    current_settings_.connection.polling_interval =
        connection_parameters.polling_interval <= 0 ? 1000 : connection_parameters.polling_interval;
}

void SettingsHolderService::set_geometry_parameters(TpcGeometryParams geometry_parameters) {
    current_settings_.geometry.length = geometry_parameters.length <= 0 ? 10 : geometry_parameters.length;
    current_settings_.geometry.radius = geometry_parameters.radius <= 0 ? 5 : geometry_parameters.radius;
}

void SettingsHolderService::set_sensors_parameters(std::vector<SensorInfo> sensors_info) {
    validate_sensors_parameters(sensors_info);
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

void SettingsHolderService::validate_and_confirm_settings(models::AppSettings settings) {
    current_settings_.connection.endpoint =
        settings.connection.endpoint.empty() ? "opc.tcp://127.0.0.1:1234" : settings.connection.endpoint;
    current_settings_.connection.polling_interval =
        settings.connection.polling_interval <= 0 ? 1000 : settings.connection.polling_interval;

    current_settings_.geometry.length = settings.geometry.length <= 0 ? 10 : settings.geometry.length;
    current_settings_.geometry.radius = settings.geometry.radius <= 0 ? 5 : settings.geometry.radius;

    validate_sensors_parameters(settings.sensors_info, true);

    apply_settings();
}

void SettingsHolderService::validate_sensors_parameters(std::vector<SensorInfo> sensors_info, bool replace_coordinates) {
    std::vector<SensorInfo> validated_sensors_info{};

    if (sensors_info.empty()) {
        return;
    }

    if (current_settings_.sensors_info.empty()) {
        current_settings_.sensors_info = sensors_info;
        return;
    }

    for (const auto& sensor : sensors_info) {
        auto it = std::find_if(
            current_settings_.sensors_info.begin(),
            current_settings_.sensors_info.end(),
            [&sensor](const SensorInfo& current_sensor) {
                return sensor.previewable_name == current_sensor.previewable_name;
            }
        );

        if (it != current_settings_.sensors_info.end()) {
            it->name = sensor.name;

            if (replace_coordinates) {
                it->x = sensor.x;
                it->y = sensor.y;
                it->z = sensor.z;
            }

            validated_sensors_info.push_back(*it);
        } else {
            validated_sensors_info.push_back(sensor);
        }
    }

    current_settings_.sensors_info = std::move(validated_sensors_info);
}
#pragma endregion

#pragma region Private methods
void SettingsHolderService::initialization_data_received(std::vector<SensorName> discovery_sensors_names) {
    if (discovery_sensors_names.empty())
        return;

    std::vector<SensorInfo> validated_sensors_info;
    validated_sensors_info.reserve(discovery_sensors_names.size());

    std::ranges::sort(discovery_sensors_names, {}, [](const SensorName& sensor) {
        return std::pair{static_cast<char>(sensor.id), sensor.number};
    });

    std::vector<SensorInfo> sensors_info;
    sensors_info.reserve(discovery_sensors_names.size());

    std::ranges::transform(discovery_sensors_names, std::back_inserter(sensors_info), [](const SensorName& sensor) {
        SensorInfo info;
        info.set_name(sensor);
        info.x = 0.0F;
        info.y = 0.0F;
        info.z = 0.0F;
        return info;
    });

    load_settings();

    validate_sensors_parameters(sensors_info, false);

    apply_settings();
}
#pragma endregion

}  // namespace tpc_qt::services
