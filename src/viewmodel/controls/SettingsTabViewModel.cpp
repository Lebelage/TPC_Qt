#include "SettingsTabViewModel.hpp"

#include <algorithm>
#include <ranges>
#include <unordered_set>
#include <utility>

#include "services/tpc_service/tpc_service.hpp"
#include "services/settings_holder/settings_holder.hpp"

#include "models/application_settings_model.hpp"
#include "tpc_system/models/data.hpp"

namespace tpc_qt::view_models {
#pragma region Constructor/Destructor
    SettingsViewModel::SettingsViewModel(QObject *parent) : QObject(parent) {
        tpc_qt::services::TpcService::instance().initialization_data_received_.subscribe(
            [this](tpc::system::models::DiscoveryResult result) {
                on_initialization_data_received(result);
            });

        load_settings();
    }


#pragma endregion

#pragma region Properties
    double SettingsViewModel::get_tpcLength() const {
        return tpc_length_;
    }

    void SettingsViewModel::set_tpcLength(double tpcLength) {
        if (qFuzzyCompare(tpc_length_, tpcLength)) {
            return;
        }
        tpc_length_ = tpcLength;
        emit tpcLengthChanged();
    }

    double SettingsViewModel::get_tpcRadius() const {
        return tpc_radius_;
    }

    void SettingsViewModel::set_tpcRadius(double tpcRadius) {
        if (qFuzzyCompare(tpc_radius_, tpcRadius)) {
            return;
        }
        tpc_radius_ = tpcRadius;
        emit tpcRadiusChanged();
    }

    QString SettingsViewModel::get_endpoint() const {
        return endpoint_;
    }

    void SettingsViewModel::set_endpoint(const QString &endpoint) {
        if (endpoint_ == endpoint) {
            return;
        }
        endpoint_ = endpoint;
        emit endpointChanged();
    }

    int SettingsViewModel::get_pollingInterval() const {
        return polling_interval_;
    }

    void SettingsViewModel::set_pollingInterval(int pollingInterval) {
        if (polling_interval_ == pollingInterval) {
            return;
        }
        polling_interval_ = pollingInterval;
        emit pollingIntervalChanged();
    }

#pragma endregion

#pragma region Commands
    void SettingsViewModel::apply_settings_command() {
        auto &settings_service = tpc_qt::services::SettingsHolderService::instance();

        settings_service.set_connection_parameters({
            .endpoint = endpoint_.toStdString(), .polling_interval = polling_interval_
        });

        settings_service.set_geometry_parameters({.length = tpc_length_, .radius = tpc_radius_});

        settings_service.apply_settings();
    }

    void SettingsViewModel::load_settings_command() {
        services::SettingsHolderService::instance().load_settings();
    }

#pragma endregion

#pragma region Methods
    void SettingsViewModel::load_settings() {
        ///
        services::SettingsHolderService::instance().load_settings();

        models::AppSettings settings = services::SettingsHolderService::instance().get_current_settings();

        set_endpoint(QString::fromStdString(settings.connection.endpoint));
        set_pollingInterval(settings.connection.polling_interval);
        set_tpcRadius(settings.geometry.radius);
        set_tpcLength(settings.geometry.length);
    }

#pragma endregion

#pragma region Handlers
    void SettingsViewModel::on_initialization_data_received(tpc::system::models::DiscoveryResult result) {
        models::AppSettings settings = services::SettingsHolderService::instance().get_current_settings();

        std::vector<models::SensorInfo> new_sensors;

        std::vector<std::string> group_names;
        group_names.reserve(result.nodes.size());

        for (const auto& [node_id, name] : result.nodes) {
            group_names.push_back(name.substr(0, 2));
        }

        std::ranges::sort(group_names);
        const auto unique_end = std::ranges::unique(group_names).begin();
        group_names.erase(unique_end, group_names.end());

        if (!settings.sensors_info.empty()) {
            for (auto gn: group_names) {
                auto it = std::find_if(
                    settings.sensors_info.begin(),
                    settings.sensors_info.end(),
                    [&gn](const models::SensorInfo &sensor) {
                        return sensor.group_name == gn;
                    }
                );

                if (it != settings.sensors_info.end()) {
                    new_sensors.push_back(*it);
                } else {
                    new_sensors.emplace_back(gn);
                }
            }
        } else {
            for (auto gn: group_names) {
                new_sensors.emplace_back(gn);
            }
        }

        auto& settings_service = services::SettingsHolderService::instance();
        settings_service.set_sensors_parameters(std::move(new_sensors));
        settings_service.apply_settings();
    }
#pragma endregion
}
