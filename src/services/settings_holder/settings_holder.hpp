#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
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
    [[nodiscard]] std::expected<void, std::string> loadSettings(const std::filesystem::path& path);
    [[nodiscard]] std::filesystem::path settingsPath() const;
    [[nodiscard]] std::expected<void, std::string> setSensorEnabled(std::string_view name, bool enabled);

    void setConnectionParameters(models::ConnectionParameters parameters);
    void setGeometryParameters(models::TpcGeometryParams parameters);
    void setGridParameters(std::array<std::size_t, 3> grid);

    [[nodiscard]] models::AppSettings currentSettings() const;

private:
    [[nodiscard]] static models::AppSettings defaultSettings();
    [[nodiscard]] std::expected<void, std::string> restoreDefaultSettings();
    void validateAndStore(models::AppSettings settings);
    [[nodiscard]] std::expected<void, std::string> loadSettingsText(
        std::string_view text, const std::optional<std::filesystem::path>& path = std::nullopt
    );
    void mergeSensors(const std::vector<models::DiscoveredSensor>& sensors);
    void onInitializationDataReceived(std::vector<models::DiscoveredSensor> sensors);
    void onConnectionChanged(bool connected);

    EventDispatcher& events_;
    FileWorker& file_worker_;
    mutable std::mutex mutex_;
    models::AppSettings current_settings_{};
    std::uint64_t revision_{};
    bool custom_profile_{};
    bool connected_{};
    std::optional<std::vector<models::DiscoveredSensor>> discovered_sensors_;
    ScopedSubscription<std::vector<models::DiscoveredSensor>> initialization_subscription_;
    ScopedSubscription<bool> connection_subscription_;
};

}  // namespace tpc_slint::services
