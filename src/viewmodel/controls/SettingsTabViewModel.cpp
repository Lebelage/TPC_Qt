#include "SettingsTabViewModel.hpp"

#include "models/application_settings.h"
#include "services/tpc_service/tpc_service.hpp"
#include "services/settings_holder/settings_holder.hpp"

namespace tpc_qt::view_models {
#pragma region Constructor/Destructor
    SettingsViewModel::SettingsViewModel(QObject *parent) : QObject(parent) {
        auto settings = tpc_qt::services::SettingsHolderService::instance().get_current_settings();
        set_endpoint(QString::fromStdString(settings.connection.endpoint));
        set_pollingInterval(settings.connection.polling_interval);
        set_tpcRadius(settings.geometric.radius);
        set_tpcLength(settings.geometric.length);
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

    void SettingsViewModel::set_endpoint(const QString& endpoint) {
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
        tpc_qt::services::SettingsHolderService::instance().apply_settings(
            {{get_tpcLength(), get_tpcRadius()}, {get_endpoint().toStdString(), get_pollingInterval()}});
    }
#pragma endregion

#pragma region Methods
#pragma endregion

#pragma region Handlers
#pragma endregion
}
