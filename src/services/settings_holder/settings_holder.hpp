#pragma once

#include <expected>
#include <string>
#include <vector>

#include "models/application_settings_model.hpp"
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

    ConnectionParameters get_connection_parameters();
    TpcGeometryParams get_geometry_parameters();
    std::vector<SensorInfo> get_sensors_parameters();

    [[nodiscard]] const models::AppSettings& get_current_settings() const noexcept;

private:
    models::AppSettings initialize_by_defaults();

    std::vector<SensorInfo> validate_sensors_parameters(std::vector<SensorInfo> sensors_parameters);

private:
void initialization_data_received(tpc::system::models::DiscoveryResult discovery_result);

private:
    models::AppSettings current_settings_{};
};
}  // namespace tpc_qt::services
