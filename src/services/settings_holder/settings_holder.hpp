#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <mutex>
#include <string>
#include <vector>

#include "models/application_settings_model.hpp"
#include "services/scoped_subscription.hpp"

namespace tpc_slint::services {

class EventDispatcher;
class FileWorker;

/** Loads, validates, persists, and publishes the current application settings. */
class SettingsHolderService final {
public:
    SettingsHolderService(EventDispatcher& events, FileWorker& file_worker);

    SettingsHolderService(const SettingsHolderService&) = delete;
    SettingsHolderService& operator=(const SettingsHolderService&) = delete;
    SettingsHolderService(SettingsHolderService&&) = delete;
    SettingsHolderService& operator=(SettingsHolderService&&) = delete;

    ~SettingsHolderService() = default;

    void applySettings();
    [[nodiscard]] std::expected<void, std::string> loadSettings();

    void setConnectionParameters(models::ConnectionParameters parameters);
    void setGeometryParameters(models::TpcGeometryParams parameters);
    void setGridParameters(std::array<std::size_t, 3> grid);

    [[nodiscard]] models::AppSettings currentSettings() const;

private:
    [[nodiscard]] static models::AppSettings defaultSettings();
    [[nodiscard]] std::expected<void, std::string> restoreDefaultSettings();
    static void setDefaultSensorPositions(
        std::vector<models::SensorInfo>& sensors,
        models::TpcGeometryParams geometry
    );
    void validateAndStore(models::AppSettings settings);
    void mergeSensors(const std::vector<models::SensorInfo>& sensors, bool replace_coordinates);
    void onInitializationDataReceived(std::vector<models::SensorName> sensor_names);

    EventDispatcher& events_;
    FileWorker& file_worker_;
    mutable std::mutex mutex_;
    models::AppSettings current_settings_{};
    ScopedSubscription<std::vector<models::SensorName>> initialization_subscription_;
};

}  // namespace tpc_slint::services
