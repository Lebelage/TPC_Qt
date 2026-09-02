#pragma once
#include <QObject>
#include <QtQmlIntegration>

#include "models/sensors_table_model.hpp"
#include "services/tpc_service/tpc_service.hpp"

namespace tpc_qt::view_models {
    class WorkspaceViewModel : public QObject {
        Q_OBJECT

        Q_PROPERTY(QAbstractItemModel* sensors_model READ get_sensors_model CONSTANT)

    public:
        explicit WorkspaceViewModel(QObject *parent = nullptr);

    public:
        QAbstractItemModel *get_sensors_model() noexcept;

        tpc_qt::models::SensorsTableModel &model() noexcept { return sensors_model_; }

    public:
        Q_INVOKABLE void try_get_frame() {
            auto result = tpc_qt::services::TpcService::instance().get_frame_request();

            if (!result) return;

            for (auto f : result.value()) {
                sensors_model_.add_sensor(QString::fromStdString(f.first), f.second);
            }
        }
    private:
       // void on_data_received(tpc_qt::models::){tpc_qt::services::TpcService::};

    private:
        tpc_qt::models::SensorsTableModel sensors_model_;
    };
}
