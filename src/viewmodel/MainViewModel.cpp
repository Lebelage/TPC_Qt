#include "MainViewModel.hpp"

#include <QObject>

#include "services/event_dispatcher/event_dispatcher.hpp"

namespace tpc_qt::view_models {

    MainViewModel::MainViewModel(QObject *parent) : QObject(parent)
        , workspace_(new WorkspaceViewModel(this))
        , bottom_bar_(new BottomBarViewModel(this, models::ui::TabsIndexingModel::Workspace))
        , settings_(new SettingsViewModel(this)) {
        {
            services::EventDispatcher::instance().tab_change_requested.subscribe(
                [this](models::ui::TabsIndexingModel target_tab) {
                    on_tab_change_requested(target_tab);
                }
            );
        }
    }
}
