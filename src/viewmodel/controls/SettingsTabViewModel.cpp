#include "SettingsTabViewModel.hpp"

#include <algorithm>
#include <cstddef>

#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/settings_holder/settings_holder.hpp"

namespace tpc_qt::view_models {

SettingsViewModel::SettingsViewModel(
    services::EventDispatcher& events,
    services::SettingsHolderService& settings,
    QObject* parent
) : QObject(parent), settings_service_(settings) {
    settings_subscription_.subscribe(
        events.settings_changed,
        [this](const models::AppSettings& settings) {
            QMetaObject::invokeMethod(this, [this, settings] { onSettingsChanged(settings); });
        }
    );
}

void SettingsViewModel::setTpcLength(double value) {
    if (!qFuzzyCompare(tpc_length_, value)) {
        tpc_length_ = value;
        Q_EMIT tpcLengthChanged();
    }
}

void SettingsViewModel::setTpcRadius(double value) {
    if (!qFuzzyCompare(tpc_radius_, value)) {
        tpc_radius_ = value;
        Q_EMIT tpcRadiusChanged();
    }
}

void SettingsViewModel::setEndpoint(const QString& value) {
    if (endpoint_ != value) {
        endpoint_ = value;
        Q_EMIT endpointChanged();
    }
}

void SettingsViewModel::setPollingInterval(int value) {
    if (polling_interval_ != value) {
        polling_interval_ = value;
        Q_EMIT pollingIntervalChanged();
    }
}

void SettingsViewModel::setGridNx(int value) {
    if (grid_nx_ != value) {
        grid_nx_ = value;
        Q_EMIT gridNxChanged();
    }
}

void SettingsViewModel::setGridNy(int value) {
    if (grid_ny_ != value) {
        grid_ny_ = value;
        Q_EMIT gridNyChanged();
    }
}

void SettingsViewModel::setGridNz(int value) {
    if (grid_nz_ != value) {
        grid_nz_ = value;
        Q_EMIT gridNzChanged();
    }
}

void SettingsViewModel::applySettings() {
    settings_service_.setConnectionParameters({endpoint_.toStdString(), polling_interval_});
    settings_service_.setGeometryParameters({tpc_length_, tpc_radius_});
    settings_service_.setGridParameters({
        static_cast<std::size_t>(std::max(grid_nx_, 1)),
        static_cast<std::size_t>(std::max(grid_ny_, 1)),
        static_cast<std::size_t>(std::max(grid_nz_, 1))
    });
    settings_service_.applySettings();
}

void SettingsViewModel::loadSettings() {
    (void)settings_service_.loadSettings();
}

void SettingsViewModel::onSettingsChanged(models::AppSettings settings) {
    setEndpoint(QString::fromStdString(settings.connection.endpoint));
    setPollingInterval(settings.connection.polling_interval);
    setTpcLength(settings.geometry.length);
    setTpcRadius(settings.geometry.radius);
    setGridNx(static_cast<int>(settings.grid[0]));
    setGridNy(static_cast<int>(settings.grid[1]));
    setGridNz(static_cast<int>(settings.grid[2]));
}

}  // namespace tpc_qt::view_models
