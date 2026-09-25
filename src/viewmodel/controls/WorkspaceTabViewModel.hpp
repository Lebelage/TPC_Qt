#pragma once
#include <QObject>

#include "models/application_settings_model.hpp"
#include "models/tables/sensors_table_model.hpp"
#include "services/scoped_subscription.hpp"

namespace tpc_qt::services {
class EventDispatcher;
class TpcService;
}

namespace tpc_qt::view_models {
/** Exposes live sensor values and field-calculation commands to QML. */
class WorkspaceViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* sensorsModel READ sensorsModel CONSTANT)
    Q_PROPERTY(QString fieldCalculationInfoStatus READ fieldCalculationInfoStatus NOTIFY fieldCalculationInfoStatusChanged)
    Q_PROPERTY(bool fieldCalculated READ fieldCalculated NOTIFY fieldCalculatedChanged)

public:
    WorkspaceViewModel(
        services::EventDispatcher& events,
        services::TpcService& tpc,
        QObject* parent = nullptr
    );

    [[nodiscard]] QAbstractItemModel* sensorsModel() noexcept;

    [[nodiscard]] QString fieldCalculationInfoStatus() const noexcept;
    [[nodiscard]] bool fieldCalculated() const noexcept;
    void setFieldCalculationInfoStatus(bool status) noexcept;

    Q_INVOKABLE void calculateField();
    Q_INVOKABLE void saveFieldAsVtk();

Q_SIGNALS:
    void fieldCalculationInfoStatusChanged();
    void fieldCalculatedChanged();

private:
    void onSettingsChanged(models::AppSettings settings);
    void onFrameReceived(models::TpcDataModel::ReceivedFrame frame);
    void onFieldWasCalculated(bool);

private:
    services::TpcService& tpc_;
    QString field_calculation_info_status_ = "Field is not calculated";
    bool field_calculated_{false};
    models::SensorsTableModel sensors_model_;

    services::ScopedSubscription<const models::AppSettings&> settings_subscription_;
    services::ScopedSubscription<const std::unordered_map<std::string, double>&> frame_subscription_;
    services::ScopedSubscription<bool> field_was_calculated_;
};

}  // namespace tpc_qt::view_models
