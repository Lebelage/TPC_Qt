#include "services/settings_holder/settings_holder.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "nlohmann/json.hpp"
#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/file_worker/file_worker.hpp"

namespace tpc_slint::services {
namespace {
constexpr std::string_view kDefaultEndpoint = "opc.tcp://127.0.0.1:1234";
constexpr int kDefaultPollingIntervalMs = 1000;
constexpr std::size_t kDefaultGridSize = 32;
// Initial calculation volume bounded by the measured sensor ring and end planes.
constexpr double kDefaultLengthMm = 4058.0;
constexpr double kDefaultRadiusMm = 1408.569;
struct SensorPosition {
    std::string_view name;
    float x, y, z;
};
// sensors_coords.txt: Cartesian millimetres; W is +Z, E is -Z.
constexpr std::array kSensorPositions{
    SensorPosition{"W1", 0.000f, 1408.569f, 2029.000f},
    SensorPosition{"W2", -1219.856f, 704.285f, 2029.000f},
    SensorPosition{"W3", -1219.856f, -704.285f, 2029.000f},
    SensorPosition{"W4", 0.000f, -1408.569f, 2029.000f},
    SensorPosition{"W5", 1219.856f, -704.285f, 2029.000f},
    SensorPosition{"W6", 1219.856f, 704.285f, 2029.000f},
    SensorPosition{"E1", 0.000f, 1408.569f, -2029.000f},
    SensorPosition{"E2", -1219.856f, 704.285f, -2029.000f},
    SensorPosition{"E3", -1219.856f, -704.285f, -2029.000f},
    SensorPosition{"E4", 0.000f, -1408.569f, -2029.000f},
    SensorPosition{"E5", 1219.856f, -704.285f, -2029.000f},
    SensorPosition{"E6", 1219.856f, 704.285f, -2029.000f}
};
}  // namespace

SettingsHolderService::SettingsHolderService(EventDispatcher& events, FileWorker& file_worker)
    : events_(events), file_worker_(file_worker) {
    initialization_subscription_.subscribe(
        events_.initialization_data_received,
        [this](std::vector<models::SensorName> names) { onInitializationDataReceived(std::move(names)); }
    );
}

void SettingsHolderService::applySettings() {
    models::AppSettings snapshot;
    {
        std::scoped_lock lock{mutex_};
        snapshot = current_settings_;
    }

    // Runtime settings remain usable even if persistence fails; publishing is
    // intentionally independent from the best-effort disk write.
    // Parentheses are intentional: brace initialization would select
    // json::initializer_list and serialize the settings as a one-item array.
    const nlohmann::json settings_json(snapshot);
    const auto written = file_worker_.writeSettings(settings_json.dump(4));
    if (!written) events_.error_occurred.invoke("Settings persistence failed: " + written.error());
    events_.settings_changed.invoke(snapshot);
}

std::expected<void, std::string> SettingsHolderService::loadSettings() {
    const auto loaded = file_worker_.loadSettings();
    if (!loaded) {
        return restoreDefaultSettings();
    }

    try {
        auto settings_json = nlohmann::json::parse(*loaded);

        // Migrate files written by the old json{settings} expression, which
        // wrapped the settings object in a one-item JSON array.
        if (settings_json.is_array() && settings_json.size() == 1 && settings_json.front().is_object()) {
            auto object = settings_json.front();
            settings_json = std::move(object);
        }
        if (!settings_json.is_object()) {
            throw std::runtime_error{"Settings root must be a JSON object"};
        }

        auto settings = settings_json.get<models::AppSettings>();
        // Files predating coordinate_unit used centimetres. Migrate once and
        // persist mm so subsequent loads cannot scale the coordinates again.
        const auto unit = settings_json.value("coordinate_unit", std::string{"cm"});
        if (unit == "cm") {
            const bool old_example_geometry =
                (settings.geometry.length == 7.0 && settings.geometry.radius == 4.0)
                || (settings.geometry.length == 10.0 && settings.geometry.radius == 5.0);
            settings.geometry.length *= 10.0;
            settings.geometry.radius *= 10.0;
            for (auto& sensor : settings.sensors_info) {
                sensor.x *= 10.f;
                sensor.y *= 10.f;
                sensor.z *= 10.f;
            }
            // Replace the former example layout with supplied measurements.
            setDefaultSensorPositions(settings.sensors_info);
            if (old_example_geometry) {
                settings.geometry = {kDefaultLengthMm, kDefaultRadiusMm};
            }
        } else if (unit != "mm") {
            throw std::runtime_error{"Settings coordinate_unit must be mm or cm"};
        }
        settings.coordinate_unit = "mm";
        {
            std::scoped_lock lock{mutex_};
            validateAndStore(std::move(settings));
        }
        applySettings();
        return {};
    } catch (const std::exception& error) {
        // Never overwrite a malformed scientific configuration with invented geometry.
        return std::unexpected("Invalid settings; file preserved: " + std::string{error.what()});
    }
}

void SettingsHolderService::setConnectionParameters(models::ConnectionParameters parameters) {
    std::scoped_lock lock{mutex_};
    current_settings_.connection.endpoint =
        parameters.endpoint.empty() ? std::string{kDefaultEndpoint} : std::move(parameters.endpoint);
    current_settings_.connection.polling_interval =
        parameters.polling_interval > 0 ? parameters.polling_interval : kDefaultPollingIntervalMs;
}

void SettingsHolderService::setGeometryParameters(models::TpcGeometryParams parameters) {
    std::scoped_lock lock{mutex_};
    current_settings_.geometry.length = parameters.length > 0.0 ? parameters.length : kDefaultLengthMm;
    current_settings_.geometry.radius = parameters.radius > 0.0 ? parameters.radius : kDefaultRadiusMm;
}

void SettingsHolderService::setGridParameters(std::array<std::size_t, 3> grid) {
    for (auto& dimension : grid) {
        if (dimension == 0) {
            dimension = kDefaultGridSize;
        }
    }

    std::scoped_lock lock{mutex_};
    current_settings_.grid = grid;
}

models::AppSettings SettingsHolderService::currentSettings() const {
    std::scoped_lock lock{mutex_};
    return current_settings_;
}

models::AppSettings SettingsHolderService::defaultSettings() {
    models::AppSettings settings{
        .geometry = {kDefaultLengthMm, kDefaultRadiusMm},
        .connection = {std::string{kDefaultEndpoint}, kDefaultPollingIntervalMs},
        .sensors_info = {},
        .grid = {kDefaultGridSize, kDefaultGridSize, kDefaultGridSize}
    };
    for (const auto& position : kSensorPositions) {
        models::SensorInfo sensor;
        sensor.setName(*models::SensorName::parse(position.name));
        sensor.x = position.x;
        sensor.y = position.y;
        sensor.z = position.z;
        settings.sensors_info.push_back(sensor);
    }
    return settings;
}

std::expected<void, std::string> SettingsHolderService::restoreDefaultSettings() {
    models::AppSettings defaults = defaultSettings();
    {
        std::scoped_lock lock{mutex_};
        current_settings_ = defaults;
    }

    const nlohmann::json settings_json(defaults);
    const auto written = file_worker_.writeSettings(settings_json.dump(4));
    events_.settings_changed.invoke(defaults);
    if (!written) {
        return std::unexpected{written.error()};
    }
    return {};
}

void SettingsHolderService::setDefaultSensorPositions(std::vector<models::SensorInfo>& sensors) {
    for (auto& sensor : sensors) {
        const auto position = std::ranges::find(kSensorPositions, sensor.previewable_name, &SensorPosition::name);
        if (position == kSensorPositions.end()) continue;
        sensor.x = position->x;
        sensor.y = position->y;
        sensor.z = position->z;
    }
}

void SettingsHolderService::validateAndStore(models::AppSettings settings) {
    const auto& limits = settings.analysis;
    if (!std::isfinite(limits.maximum_residual_gauss) || limits.maximum_residual_gauss <= 0
        || !std::isfinite(limits.maximum_radial_ratio) || limits.maximum_radial_ratio <= 0
        || !std::isfinite(limits.minimum_axial_field_gauss) || limits.minimum_axial_field_gauss <= 0
        || limits.maximum_sample_age_ms <= 0 || limits.maximum_frame_skew_ms < 0)
        throw std::runtime_error{"Invalid scientific reconstruction limits"};
    settings.connection.endpoint = settings.connection.endpoint.empty()
        ? std::string{kDefaultEndpoint}
        : std::move(settings.connection.endpoint);
    settings.connection.polling_interval = settings.connection.polling_interval > 0
        ? settings.connection.polling_interval
        : kDefaultPollingIntervalMs;
    if (!std::isfinite(settings.geometry.length) || !std::isfinite(settings.geometry.radius)
        || settings.geometry.length <= 0 || settings.geometry.radius <= 0)
        throw std::runtime_error{"Invalid TPC geometry; measured dimensions are required"};

    for (auto& dimension : settings.grid) {
        if (dimension < 2 || dimension > 1024)
            throw std::runtime_error{"Grid dimensions must be between 2 and 1024"};
    }

    // SensorName itself is not serialized. Reconstruct it from the persisted
    // display name so consumers can use saved sensors before OPC discovery.
    for (auto& sensor : settings.sensors_info) {
        if (!std::isfinite(sensor.x) || !std::isfinite(sensor.y) || !std::isfinite(sensor.z))
            throw std::runtime_error{"Sensor coordinates must be finite"};
        if (const auto name = models::SensorName::parse(sensor.previewable_name)) {
            sensor.name = *name;
        } else {
            throw std::runtime_error{"Invalid sensor identity in settings"};
        }
    }
    // Publish only after every field is validated, preserving the previous
    // runtime configuration when a reload fails midway.
    current_settings_ = std::move(settings);
}

void SettingsHolderService::mergeSensors(
    const std::vector<models::SensorInfo>& sensors,
    bool replace_coordinates
) {
    if (sensors.empty()) {
        return;
    }

    if (current_settings_.sensors_info.empty()) {
        current_settings_.sensors_info = sensors;
        return;
    }

    std::vector<models::SensorInfo> merged;
    merged.reserve(sensors.size());

    for (const auto& sensor : sensors) {
        const auto saved = std::ranges::find(
            current_settings_.sensors_info, sensor.previewable_name, &models::SensorInfo::previewable_name
        );

        if (saved == current_settings_.sensors_info.end()) {
            merged.push_back(sensor);
            continue;
        }

        auto result = *saved;
        result.name = replace_coordinates ? saved->name : sensor.name;
        if (replace_coordinates) {
            result.x = sensor.x;
            result.y = sensor.y;
            result.z = sensor.z;
        }
        merged.push_back(std::move(result));
    }

    current_settings_.sensors_info = std::move(merged);
}

void SettingsHolderService::onInitializationDataReceived(std::vector<models::SensorName> sensor_names) {
    if (sensor_names.empty()) {
        return;
    }

    std::ranges::sort(sensor_names, {}, [](const models::SensorName& sensor) {
        return std::pair{static_cast<char>(sensor.id), sensor.number};
    });

    std::vector<models::SensorInfo> sensors;
    sensors.reserve(sensor_names.size());
    std::ranges::transform(sensor_names, std::back_inserter(sensors), [](const models::SensorName& sensor) {
        models::SensorInfo info;
        info.setName(sensor);
        return info;
    });

    {
        std::scoped_lock lock{mutex_};
        setDefaultSensorPositions(sensors);
        mergeSensors(sensors, false);
    }

    applySettings();
}

}  // namespace tpc_slint::services
