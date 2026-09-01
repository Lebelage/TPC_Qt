#include "WorkspaceTabViewModel.hpp"

namespace tpc_qt::view_models {
#pragma region Constructor/Destructor
    WorkspaceViewModel::WorkspaceViewModel(QObject *parent) : QObject(parent) {
    }
#pragma endregion

#pragma region Properties

#pragma region [Properties] : sensors_model
    QAbstractItemModel *WorkspaceViewModel::get_sensors_model() noexcept  { return &sensors_model_; }
#pragma endregion

#pragma endregion

#pragma region Commands
#pragma endregion

#pragma region Methods
#pragma endregion

#pragma region Handlers
#pragma endregion
}
