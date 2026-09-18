#pragma once

#include <expected>
#include <string>
#include <vector>

#include "models/application_settings_model.hpp"
#include "models/tpc_data_model.hpp"
#include "tpc_system/models/data.hpp"
using namespace tpc_qt::models;

namespace tpc_qt::services {
struct AppSettings;

class SettingsHolderService {
public:
    static SettingsHolderService& instance();

    SettingsHolderService(const SettingsHolderService&) = delete;

    SettingsHolderService& operator=(const SettingsHolderService&) = delete;

    SettingsHolderService(SettingsHolderService&&) = delete;

    SettingsHolderService& operator=(SettingsHolderService&&) = delete;

    ~SettingsHolderService();

private:
    SettingsHolderService();

public:
    void apply_settings();

    std::expected<void, std::string> load_settings();

    void set_connection_parameters(ConnectionParameters);
    void set_geometry_parameters(TpcGeometryParams);
    void set_sensors_parameters(std::vector<SensorInfo>);

    [[nodiscard]] const models::AppSettings& get_current_settings() const noexcept;

private:
    models::AppSettings initialize_by_defaults();

    void validate_and_confirm_settings(models::AppSettings settings);

    void validate_sensors_parameters(std::vector<SensorInfo> sensors_parameters, bool replace_coordinates = false);

    void validate_settings();

private:
void initialization_data_received(std::vector<SensorName> discovery_sensors_names);

private:
    models::AppSettings current_settings_{};
};
}  // namespace tpc_qt::services
