#include "WorkspaceTabViewModel.hpp"

#include <utility>
#include <vector>

#include "services/file_dialog/file_dialog_service.hpp"
#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/tpc_service/tpc_service.hpp"

namespace tpc_qt::view_models {
WorkspaceViewModel::WorkspaceViewModel(
    services::EventDispatcher& events,
    services::TpcService& tpc,
    QObject* parent
) : QObject(parent), tpc_(tpc) {
    settings_subscription_.subscribe(
        events.settings_changed,
        [this](const models::AppSettings& settings) {
            QMetaObject::invokeMethod(this, [this, settings] {
                onSettingsChanged(settings);
            });
        }
    );

    frame_subscription_.subscribe(
        events.frame_received,
        [this](const std::unordered_map<std::string, double> frame) {
            QMetaObject::invokeMethod(this, [this, frame = std::move(frame)] {
                onFrameReceived(std::move(frame));
            });
        }
    );

    field_was_calculated_.subscribe(
        events.field_was_calculated_,
        [this](bool is_calculated) {
            QMetaObject::invokeMethod(this, [this, is_calculated = std::move(is_calculated)] {
                onFieldWasCalculated(is_calculated);
            });
        }
    );
}

QAbstractItemModel* WorkspaceViewModel::sensorsModel() noexcept {
    return &sensors_model_;
}
QString WorkspaceViewModel::fieldCalculationInfoStatus() const noexcept {
    return field_calculation_info_status_;
}

bool WorkspaceViewModel::fieldCalculated() const noexcept {
    return field_calculated_;
}

void WorkspaceViewModel::setFieldCalculationInfoStatus(bool status) noexcept {
    if (field_calculated_ == status) {
        return;
    }

    field_calculated_ = status;
    field_calculation_info_status_ = status ? "Field is calculated" : "Field is not calculated";
    Q_EMIT fieldCalculationInfoStatusChanged();
    Q_EMIT fieldCalculatedChanged();
}

void WorkspaceViewModel::calculateField() {
    setFieldCalculationInfoStatus(false);
    tpc_.calculateField();
}

void WorkspaceViewModel::saveFieldAsVtk() {
    if (!field_calculated_) {
        return;
    }

    const auto path = services::FileDialogService::saveFile(nullptr, "Field", "VTK files (*.vtk)");

    if (!path) {
        return;
    }

    (void)tpc_.exportFieldToVtk(path->string());
}

void WorkspaceViewModel::onSettingsChanged(models::AppSettings settings) {
    setFieldCalculationInfoStatus(false);

    std::vector<models::Sensor> sensors;
    sensors.reserve(settings.sensors_info.size());

    for (const auto& info : settings.sensors_info) {
        sensors.push_back({
            .name = info.name,
            .position = {info.x, info.y, info.z}
        });
    }

    sensors_model_.setSensors(std::move(sensors));
}

void WorkspaceViewModel::onFrameReceived(models::TpcDataModel::ReceivedFrame frame) {
    sensors_model_.applyFrame(frame);
}
void WorkspaceViewModel::onFieldWasCalculated(bool is_calculated) {
    setFieldCalculationInfoStatus(is_calculated);
}

}  // namespace tpc_qt::view_models
