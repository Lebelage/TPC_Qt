#include "WorkspaceTabViewModel.hpp"

#include "services/settings_holder/settings_holder.hpp"
#include "services/tpc_service/tpc_service.hpp"
#include "tpc_system/models/data.hpp"
#include "services/file_dialog/file_dialog_service.hpp"
namespace tpc_qt::view_models {
#pragma region Constructor/Destructor
WorkspaceViewModel::WorkspaceViewModel(QObject* parent) : QObject(parent) {
    tpc_qt::services::TpcService::instance().initialization_data_received_.subscribe(
        [this](tpc::system::models::DiscoveryResult discovery_result) {
            on_data_initialization_data_received(discovery_result);
        }
    );
}
#pragma endregion

#pragma region Properties

#pragma region[Properties] : sensors_model
QAbstractItemModel* WorkspaceViewModel::get_sensors_model() noexcept {
    return &sensors_model_;
}

#pragma endregion

#pragma endregion

#pragma region Commands
void WorkspaceViewModel::get_frame_command() {
    auto result = tpc_qt::services::TpcService::instance().get_frame_request();

    if (!result)
        return;

    for (auto frame : result.value()) {
        sensors_model_.set_value(QString::fromStdString(frame.first), frame.second);
    }
}

void WorkspaceViewModel::calculate_field_command() {
    tpc_qt::services::TpcService::instance().calculate_field_3d();
}

void WorkspaceViewModel::save_filed_vtk_as() {
    auto result = services::FileDialogService::save_file(nullptr, "Field", "VTK files (*.vtk)");

    if (!result)
        return;

    services::TpcService::instance().export_field_to_vtk(result.value().c_str());
}

void WorkspaceViewModel::save_filed_vtk() {}
#pragma endregion

#pragma region Methods

void WorkspaceViewModel::initialize(tpc::system::models::DiscoveryResult discovery_result) {
    for (auto frame : discovery_result.nodes) {
        sensors_model_.add_sensor(QString::fromStdString(frame.second), 0.);
    }
}
#pragma endregion

#pragma region Handlers

void WorkspaceViewModel::on_data_initialization_data_received(tpc::system::models::DiscoveryResult discovery_result) {
    initialize(discovery_result);
}
#pragma endregion
}  // namespace tpc_qt::view_models
