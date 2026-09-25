#include "MainViewModel.hpp"

#include "services/event_dispatcher/event_dispatcher.hpp"

namespace tpc_qt::view_models {

MainViewModel::MainViewModel(
    services::EventDispatcher& events,
    services::SettingsHolderService& settings,
    services::TpcService& tpc,
    services::FieldSliceService& field_slices,
    QObject* parent
)
    : QObject(parent),
      workspace_(events, tpc),
      bottom_bar_(models::ui::TabsIndexingModel::Workspace, events, settings, tpc),
      settings_(events, settings),
      field_visualization_(field_slices) {
    tab_subscription_.subscribe(
        events.tab_change_requested,
        [this](models::ui::TabsIndexingModel target_tab) { onTabChangeRequested(target_tab); }
    );
}

void MainViewModel::onTabChangeRequested(models::ui::TabsIndexingModel target_tab) {
    if (current_tab_ == target_tab) {
        return;
    }

    current_tab_ = target_tab;
    Q_EMIT currentTabIndexChanged();
}

}  // namespace tpc_qt::view_models
