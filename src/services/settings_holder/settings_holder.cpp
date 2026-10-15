#include "services/settings_holder/settings_holder.hpp"

#include <cmath>

#include <algorithm>
#include <exception>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
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
    SensorPosition{"W1",     0.000f,  1408.569f,  2029.000f},
    SensorPosition{"W2", -1219.856f,   704.285f,  2029.000f},
    SensorPosition{"W3", -1219.856f,  -704.285f,  2029.000f},
    SensorPosition{"W4",     0.000f, -1408.569f,  2029.000f},
    SensorPosition{"W5",  1219.856f,  -704.285f,  2029.000f},
    SensorPosition{"W6",  1219.856f,   704.285f,  2029.000f},
    SensorPosition{"E1",     0.000f,  1408.569f, -2029.000f},
    SensorPosition{"E2", -1219.856f,   704.285f, -2029.000f},
    SensorPosition{"E3", -1219.856f,  -704.285f, -2029.000f},
    SensorPosition{"E4",     0.000f, -1408.569f, -2029.000f},
    SensorPosition{"E5",  1219.856f,  -704.285f, -2029.000f},
    SensorPosition{"E6",  1219.856f,   704.285f, -2029.000f}
};
bool setDefaultSensorPosition(models::SensorInfo& sensor) {
    const auto position = std::ranges::find(kSensorPositions, sensor.previewable_name, &SensorPosition::name);
    if (position == kSensorPositions.end())
        return false;
    sensor.x = position->x;
    sensor.y = position->y;
    sensor.z = position->z;
    return true;
}
}  // namespace

SettingsHolderService::SettingsHolderService(EventDispatcher& events, FileWorker& file_worker)
    : events_(events), file_worker_(file_worker) {
    initialization_subscription_.subscribe(
        events_.initialization_data_received, [this](std::vector<models::DiscoveredSensor> sensors) {
            onInitializationDataReceived(std::move(sensors));
        }
    );
    connection_subscription_.subscribe(events_.connection_state_changed, [this](bool connected) {
        onConnectionChanged(connected);
    });
}

void SettingsHolderService::applySettings() {
    models::AppSettings snapshot;
    {
        std::scoped_lock lock{mutex_};
        current_settings_.runtime_revision = ++revision_;
        snapshot = current_settings_;
    }

    // Runtime settings remain usable even if persistence fails; publishing is
    // intentionally independent from the best-effort disk write.
    // Parentheses are intentional: brace initialization would select
    // json::initializer_list and serialize the settings as a one-item array.
    const nlohmann::json settings_json(snapshot);
    const auto written = file_worker_.writeSettings(settings_json.dump(4));
    if (!written)
        events_.error_occurred.invoke("Settings persistence failed: " + written.error());
    events_.settings_changed.invoke(snapshot);
}

std::expected<void, std::string> SettingsHolderService::loadSettings() {
    const auto loaded = file_worker_.loadSettings();
    if (!loaded) {
        {
            std::scoped_lock lock{mutex_};
            if (custom_profile_)
                return std::unexpected(loaded.error());
        }
        return restoreDefaultSettings();
    }

    return loadSettingsText(*loaded);
}

std::expected<void, std::string> SettingsHolderService::loadSettings(const std::filesystem::path& path) {
    std::error_code error;
    const auto absolute_path = std::filesystem::absolute(path, error);
    if (error)
        return std::unexpected("Invalid configuration path: " + error.message());
    const auto loaded = FileWorker::readSettings(absolute_path);
    if (!loaded)
        return std::unexpected(loaded.error());
    return loadSettingsText(*loaded, absolute_path);
}

std::filesystem::path SettingsHolderService::settingsPath() const {
    return file_worker_.settingsPath();
}

std::expected<void, std::string> SettingsHolderService::loadSettingsText(
    std::string_view text, const std::optional<std::filesystem::path>& path
) {
    try {
        auto settings_json = nlohmann::json::parse(text);

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
            const bool old_example_geometry = (settings.geometry.length == 7.0 && settings.geometry.radius == 4.0) ||
                                              (settings.geometry.length == 10.0 && settings.geometry.radius == 5.0);
            settings.geometry.length *= 10.0;
            settings.geometry.radius *= 10.0;
            for (auto& sensor : settings.sensors_info) {
                sensor.x *= 10.f;
                sensor.y *= 10.f;
                sensor.z *= 10.f;
            }
            // Replace the former example layout with supplied measurements.
            if (!settings_json.contains("coordinate_unit") && old_example_geometry) {
                for (auto& sensor : settings.sensors_info)
                    setDefaultSensorPosition(sensor);
                settings.geometry = {kDefaultLengthMm, kDefaultRadiusMm};
            }
        } else if (unit != "mm") {
            throw std::runtime_error{"Settings coordinate_unit must be mm or cm"};
        }
        settings.coordinate_unit = "mm";
        for (std::size_t index = 0; index < settings.sensors_info.size(); ++index) {
            auto& sensor = settings.sensors_info[index];
            const auto& source = settings_json.at("sensors_info").at(index);
            const int coordinates = int(source.contains("x")) + int(source.contains("y")) + int(source.contains("z"));
            if (coordinates != 0 && coordinates != 3)
                throw std::runtime_error{"Specify all three coordinates for " + sensor.previewable_name};
            if (coordinates == 0) {
                const bool known_position = setDefaultSensorPosition(sensor);
                sensor.position_configured = source.value("position_configured", true) && known_position;
            }
        }
        if (!settings.analysis.reference_map_path.empty()) {
            std::filesystem::path reference{settings.analysis.reference_map_path};
            if (reference.is_relative())
                settings.analysis.reference_map_path =
                    ((path ? *path : file_worker_.settingsPath()).parent_path() / reference)
                        .lexically_normal()
                        .string();
        }
        models::AppSettings snapshot;
        {
            std::scoped_lock lock{mutex_};
            const bool endpoint_changed = settings.connection.endpoint != current_settings_.connection.endpoint;
            validateAndStore(std::move(settings));
            if (path) {
                discovered_sensors_.reset();
                file_worker_.setSettingsPath(*path);
                custom_profile_ = true;
            } else if (endpoint_changed) {
                discovered_sensors_.reset();
            } else if (discovered_sensors_) {
                mergeSensors(*discovered_sensors_);
            }
            current_settings_.runtime_revision = ++revision_;
            snapshot = current_settings_;
        }
        if (path)
            events_.settings_changed.invoke(snapshot);
        else
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
        defaults.runtime_revision = ++revision_;
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

void SettingsHolderService::validateAndStore(models::AppSettings settings) {
    const auto& limits = settings.analysis;
    if (!std::isfinite(limits.maximum_residual_gauss) || limits.maximum_residual_gauss <= 0 ||
        !std::isfinite(limits.maximum_radial_ratio) || limits.maximum_radial_ratio <= 0 ||
        !std::isfinite(limits.minimum_axial_field_gauss) || limits.minimum_axial_field_gauss <= 0 ||
        limits.maximum_sample_age_ms <= 0 || limits.maximum_frame_skew_ms < 0)
        throw std::runtime_error{"Invalid scientific reconstruction limits"};
    settings.connection.endpoint =
        settings.connection.endpoint.empty() ? std::string{kDefaultEndpoint} : std::move(settings.connection.endpoint);
    settings.connection.polling_interval =
        settings.connection.polling_interval > 0 ? settings.connection.polling_interval : kDefaultPollingIntervalMs;
    if (!std::isfinite(settings.geometry.length) || !std::isfinite(settings.geometry.radius) ||
        settings.geometry.length <= 0 || settings.geometry.radius <= 0)
        throw std::runtime_error{"Invalid TPC geometry; measured dimensions are required"};

    for (auto& dimension : settings.grid) {
        if (dimension < 2 || dimension > 1024)
            throw std::runtime_error{"Grid dimensions must be between 2 and 1024"};
    }

    // SensorName itself is not serialized. Reconstruct it from the persisted
    // display name so consumers can use saved sensors before OPC discovery.
    std::unordered_set<std::string> identities;
    for (auto& sensor : settings.sensors_info) {
        sensor.available = false;
        if (!std::isfinite(sensor.x) || !std::isfinite(sensor.y) || !std::isfinite(sensor.z))
            throw std::runtime_error{"Sensor coordinates must be finite"};
        if (const auto name = models::SensorName::parse(sensor.previewable_name)) {
            if (name->number == 0 || name->toString() != sensor.previewable_name ||
                !identities.insert(sensor.previewable_name).second)
                throw std::runtime_error{"Invalid or duplicate sensor identity in settings"};
            sensor.name = *name;
            for (const auto& [component, calibration] : sensor.calibration) {
                if ((component != "R" && component != "F" && component != "Z") || !std::isfinite(calibration.k) ||
                    calibration.k == 0 || !std::isfinite(calibration.v0_mv))
                    throw std::runtime_error{"Invalid calibration for " + sensor.previewable_name};
            }
        } else {
            throw std::runtime_error{"Invalid sensor identity in settings"};
        }
    }
    // Publish only after every field is validated, preserving the previous
    // runtime configuration when a reload fails midway.
    current_settings_ = std::move(settings);
}

void SettingsHolderService::mergeSensors(const std::vector<models::DiscoveredSensor>& sensors) {
    for (auto& configured : current_settings_.sensors_info)
        configured.available = false;
    for (const auto& discovered : sensors) {
        const auto name = discovered.name.toString();
        auto configured =
            std::ranges::find(current_settings_.sensors_info, name, &models::SensorInfo::previewable_name);
        if (configured == current_settings_.sensors_info.end()) {
            models::SensorInfo added;
            added.setName(discovered.name);
            added.position_configured = setDefaultSensorPosition(added);
            current_settings_.sensors_info.push_back(std::move(added));
            configured = std::prev(current_settings_.sensors_info.end());
        }
        configured->available = discovered.complete();
    }
}

void SettingsHolderService::onInitializationDataReceived(std::vector<models::DiscoveredSensor> sensors) {
    models::AppSettings snapshot;
    {
        std::scoped_lock lock{mutex_};
        if (!connected_)
            return;
        discovered_sensors_ = std::move(sensors);
        mergeSensors(*discovered_sensors_);
        current_settings_.runtime_revision = ++revision_;
        snapshot = current_settings_;
    }
    events_.settings_changed.invoke(snapshot);
}

void SettingsHolderService::onConnectionChanged(bool connected) {
    models::AppSettings snapshot;
    {
        std::scoped_lock lock{mutex_};
        connected_ = connected;
        if (connected)
            return;
        if (!discovered_sensors_)
            return;
        discovered_sensors_.reset();
        for (auto& sensor : current_settings_.sensors_info)
            sensor.available = false;
        current_settings_.runtime_revision = ++revision_;
        snapshot = current_settings_;
    }
    events_.settings_changed.invoke(snapshot);
}

std::expected<void, std::string> SettingsHolderService::setSensorEnabled(std::string_view name, bool enabled) {
    models::AppSettings snapshot;
    {
        std::scoped_lock lock{mutex_};
        auto sensor = std::ranges::find(current_settings_.sensors_info, name, &models::SensorInfo::previewable_name);
        if (sensor == current_settings_.sensors_info.end())
            return std::unexpected("Unknown sensor");
        if (!sensor->selectable())
            return std::unexpected("Sensor is unavailable or has no configured position");
        if (sensor->enabled == enabled)
            return {};
        sensor->enabled = enabled;
        current_settings_.runtime_revision = ++revision_;
        snapshot = current_settings_;
    }
    events_.settings_changed.invoke(snapshot);
    return {};
}

}  // namespace tpc_slint::services
