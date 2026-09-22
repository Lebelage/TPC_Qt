#include "SettingsTabViewModel.hpp"

#include <algorithm>
#include <ranges>
#include <unordered_set>
#include <utility>

#include "models/application_settings_model.hpp"
#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/settings_holder/settings_holder.hpp"
#include "services/tpc_service/tpc_service.hpp"
#include "tpc_system/models/data.hpp"

namespace tpc_qt::view_models {
#pragma region Constructor/Destructor
SettingsViewModel::SettingsViewModel(QObject* parent) : QObject(parent) {
    services::EventDispatcher::instance().settings_changed.subscribe([this](const models::AppSettings& settings){
        on_settings_changed(settings);
    });
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
    Q_EMIT tpcLengthChanged();
}

double SettingsViewModel::get_tpcRadius() const {
    return tpc_radius_;
}

void SettingsViewModel::set_tpcRadius(double tpcRadius) {
    if (qFuzzyCompare(tpc_radius_, tpcRadius)) {
        return;
    }
    tpc_radius_ = tpcRadius;
    Q_EMIT tpcRadiusChanged();
}

QString SettingsViewModel::get_endpoint() const {
    return endpoint_;
}

void SettingsViewModel::set_endpoint(const QString& endpoint) {
    if (endpoint_ == endpoint) {
        return;
    }
    endpoint_ = endpoint;
    Q_EMIT endpointChanged();
}

int SettingsViewModel::get_pollingInterval() const {
    return polling_interval_;
}

void SettingsViewModel::set_pollingInterval(int pollingInterval) {
    if (polling_interval_ == pollingInterval) {
        return;
    }
    polling_interval_ = pollingInterval;
    Q_EMIT pollingIntervalChanged();
}

int SettingsViewModel::get_gridNx() const {
    return grid_nx_;
}

void SettingsViewModel::set_gridNx(int gridNx) {
    if (grid_nx_ == gridNx) {
        return;
    }
    grid_nx_ = gridNx;
    Q_EMIT gridNxChanged();
}

int SettingsViewModel::get_gridNy() const {
    return grid_ny_;
}

void SettingsViewModel::set_gridNy(int gridNy) {
    if (grid_ny_ == gridNy) {
        return;
    }
    grid_ny_ = gridNy;
    Q_EMIT gridNyChanged();
}

int SettingsViewModel::get_gridNz() const {
    return grid_nz_;
}

void SettingsViewModel::set_gridNz(int gridNz) {
    if (grid_nz_ == gridNz) {
        return;
    }
    grid_nz_ = gridNz;
    Q_EMIT gridNzChanged();
}

#pragma endregion

#pragma region Commands
void SettingsViewModel::apply_settings_command() {
    auto& settings_service = tpc_qt::services::SettingsHolderService::instance();

    settings_service.set_connection_parameters(
        {.endpoint = endpoint_.toStdString(), .polling_interval = polling_interval_}
    );

    settings_service.set_geometry_parameters({.length = tpc_length_, .radius = tpc_radius_});
    settings_service.set_grid_parameters({
        static_cast<size_t>(grid_nx_),
        static_cast<size_t>(grid_ny_),
        static_cast<size_t>(grid_nz_)
    });

    settings_service.apply_settings();
}

void SettingsViewModel::load_settings_command() {
    services::SettingsHolderService::instance().load_settings();
}

#pragma endregion

#pragma region Methods

#pragma endregion

#pragma region Handlers
void SettingsViewModel::on_settings_changed(const models::AppSettings& settings){
    set_endpoint(QString::fromStdString(settings.connection.endpoint));
    set_pollingInterval(settings.connection.polling_interval);
    set_tpcLength(settings.geometry.length);
    set_tpcRadius(settings.geometry.radius);
    set_gridNx(static_cast<int>(settings.grid[0]));
    set_gridNy(static_cast<int>(settings.grid[1]));
    set_gridNz(static_cast<int>(settings.grid[2]));
}
#pragma endregion
}  // namespace tpc_qt::view_models
