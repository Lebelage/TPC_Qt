#pragma once

#include <QObject>
#include <QString>

#include "models/application_settings_model.hpp"
#include "services/scoped_subscription.hpp"

namespace tpc_qt::services {
class EventDispatcher;
class SettingsHolderService;
}

namespace tpc_qt::view_models {

/** Editable settings state exposed to the settings QML page. */
class SettingsViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(double tpcLength READ tpcLength WRITE setTpcLength NOTIFY tpcLengthChanged)
    Q_PROPERTY(double tpcRadius READ tpcRadius WRITE setTpcRadius NOTIFY tpcRadiusChanged)
    Q_PROPERTY(QString endpoint READ endpoint WRITE setEndpoint NOTIFY endpointChanged)
    Q_PROPERTY(int pollingInterval READ pollingInterval WRITE setPollingInterval NOTIFY pollingIntervalChanged)
    Q_PROPERTY(int gridNx READ gridNx WRITE setGridNx NOTIFY gridNxChanged)
    Q_PROPERTY(int gridNy READ gridNy WRITE setGridNy NOTIFY gridNyChanged)
    Q_PROPERTY(int gridNz READ gridNz WRITE setGridNz NOTIFY gridNzChanged)

public:
    SettingsViewModel(
        services::EventDispatcher& events,
        services::SettingsHolderService& settings,
        QObject* parent = nullptr
    );

    [[nodiscard]] double tpcLength() const noexcept { return tpc_length_; }
    void setTpcLength(double value);

    [[nodiscard]] double tpcRadius() const noexcept { return tpc_radius_; }
    void setTpcRadius(double value);

    [[nodiscard]] QString endpoint() const { return endpoint_; }
    void setEndpoint(const QString& value);

    [[nodiscard]] int pollingInterval() const noexcept { return polling_interval_; }
    void setPollingInterval(int value);

    [[nodiscard]] int gridNx() const noexcept { return grid_nx_; }
    void setGridNx(int value);

    [[nodiscard]] int gridNy() const noexcept { return grid_ny_; }
    void setGridNy(int value);

    [[nodiscard]] int gridNz() const noexcept { return grid_nz_; }
    void setGridNz(int value);

    Q_INVOKABLE void applySettings();
    Q_INVOKABLE void loadSettings();

Q_SIGNALS:
    void tpcLengthChanged();
    void tpcRadiusChanged();
    void endpointChanged();
    void pollingIntervalChanged();
    void gridNxChanged();
    void gridNyChanged();
    void gridNzChanged();

private:
    void onSettingsChanged(models::AppSettings settings);

    services::SettingsHolderService& settings_service_;
    QString endpoint_;
    int polling_interval_{0};
    double tpc_length_{0.0};
    double tpc_radius_{0.0};
    int grid_nx_{1};
    int grid_ny_{1};
    int grid_nz_{1};
    services::ScopedSubscription<const models::AppSettings&> settings_subscription_;
};

}  // namespace tpc_qt::view_models
