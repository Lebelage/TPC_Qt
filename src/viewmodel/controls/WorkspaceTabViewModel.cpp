#include "WorkspaceTabViewModel.hpp"

import tpc.system.models.system_data;
import tpc_qt.services.tpc_srvice;
import tpc_qt.services.settings_holder;
namespace tpc_qt::view_models {
#pragma region Constructor/Destructor
    WorkspaceViewModel::WorkspaceViewModel(QObject *parent) : QObject(parent) {
        tpc_qt::services::TpcService::instance().initialization_data_received_.subscribe(
            [this](tpc::system::models::DiscoveryResult discovery_result) {
                on_data_initialization_data_received(discovery_result);
            });
    }
#pragma endregion

#pragma region Properties

#pragma region [Properties] : sensors_model
    QAbstractItemModel *WorkspaceViewModel::get_sensors_model() noexcept { return &sensors_model_; }


#pragma endregion

#pragma endregion

#pragma region Commands
    void WorkspaceViewModel::try_get_frame_command() {
        auto result = tpc_qt::services::TpcService::instance().get_frame_request();

        if (!result)
            return;

        for (auto frame: result.value()) {
            sensors_model_.set_value(QString::fromStdString(frame.first), frame.second);
        }
    }

    void WorkspaceViewModel::try_calculate_field_command() {
        auto values_requested = services::TpcService::instance().get_frame_request();
        auto sensors_requested = services::SettingsHolderService::instance().get_current_settings().sensors_info;

        auto nodes = services::TpcService::instance().get_initialization_data().nodes;

        std::vector<double> prepared_values;
        std::vector<double> prepared_positions;

        prepared_values.reserve(nodes.size());
        prepared_positions.reserve(nodes.size());

        for(auto n : nodes) {
            prepared_values.push_back(values_requested.value()[n.second]);

            
        }
    }
#pragma endregion

#pragma region Methods

    void WorkspaceViewModel::initialize(tpc::system::models::DiscoveryResult discovery_result) {
        for (auto frame: discovery_result.nodes) {
            sensors_model_.add_sensor(QString::fromStdString(frame.second), 0.);
        }
    }
#pragma endregion

#pragma region Handlers

    void WorkspaceViewModel::on_data_initialization_data_received(
        tpc::system::models::DiscoveryResult discovery_result) {
        initialize(discovery_result);
    }
#pragma endregion
}
