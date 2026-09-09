#include "SettingsTabViewModel.hpp"

#include <unordered_set>

import tpc_qt.services.tpc_srvice;
import tpc_qt.services.settings_holder;

import tpc_qt.models.ui.application_settings_model;
import tpc.system.models.system_data;

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
        // auto settings = tpc_qt::services::SettingsHolderService::instance().get_current_settings();
        // set_endpoint(QString::fromStdString(settings.connection.endpoint));
        // set_pollingInterval(settings.connection.polling_interval);
        // set_tpcRadius(settings.geometric.radius);
        // set_tpcLength(settings.geometric.length);
        //
        // services::SettingsHolderService::instance().apply_settings(settings);
    }

    void SettingsViewModel::load_settings_command() {
        services::SettingsHolderService::instance().load_settings();

        auto loaded = services::SettingsHolderService::instance().get_current_settings();
        set_endpoint(QString::fromStdString(loaded.connection.endpoint));
        set_pollingInterval(loaded.connection.polling_interval);
        set_tpcRadius(loaded.geometric.radius);
        set_tpcLength(loaded.geometric.length);
    }

#pragma endregion

#pragma region Methods
    void SettingsViewModel::load_settings() {
        ///
        services::SettingsHolderService::instance().load_settings();

        models::AppSettings settings = services::SettingsHolderService::instance().get_current_settings();

        set_endpoint(QString::fromStdString(settings.connection.endpoint));
        set_pollingInterval(settings.connection.polling_interval);
        set_tpcRadius(settings.geometric.radius);
        set_tpcLength(settings.geometric.length);
    }

#pragma endregion

#pragma region Handlers
    void SettingsViewModel::on_initialization_data_received(tpc::system::models::DiscoveryResult result) {
        models::AppSettings settings = services::SettingsHolderService::instance().get_current_settings();

        std::vector<models::SensorInfo> new_sensors;

        std::vector<std::string> group_names;
        group_names.reserve(result.nodes.size());

        auto unique_set = result.nodes
                          | std::views::values
                          | std::views::transform([](const std::string &s) { return s.substr(0, 2); })
                          | std::ranges::to<std::unordered_set<std::string> >();

        group_names = unique_set | std::ranges::to<std::vector<std::string> >();

        if (!settings.sensors_info.sensors.empty()) {
            for (auto gn: group_names) {
                auto it = std::find_if(
                    settings.sensors_info.sensors.begin(),
                    settings.sensors_info.sensors.end(),
                    [&gn](const models::SensorInfo &sensor) {
                        return sensor.group_name == gn;
                    }
                );

                if (it != settings.sensors_info.sensors.end()) {
                    new_sensors.push_back(*it);
                } else {
                    new_sensors.emplace_back(gn);
                }
            }
        } else {
            for (auto gn: group_names) {
                settings.sensors_info.sensors.emplace_back(gn);
            }
        }

        services::SettingsHolderService::instance().apply_settings(settings);
    }
#pragma endregion
}
