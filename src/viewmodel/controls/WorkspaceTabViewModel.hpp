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
        Q_INVOKABLE void try_get_frame_command();

    private:
        void initialize(tpc::system::models::DiscoveryResult);

    private:
       void on_data_initialization_data_received(tpc::system::models::DiscoveryResult);

    private:
        tpc_qt::models::SensorsTableModel sensors_model_;
    };
}
