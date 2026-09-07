#include "MainViewModel.hpp"

#include <QObject>

import tpc_qt.services.event_dispatcher;

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
