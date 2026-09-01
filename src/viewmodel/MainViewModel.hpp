#pragma once

#include <QObject>

#include "controls/WorkspaceTabViewModel.hpp"
#include "controls/BottomBarViewModel.hpp"
#include "controls/SettingsTabViewModel.hpp"

#include "models/ui/tabs_indexing_model.hpp"
#include "services/event_dispatcher/event_dispatcher.hpp"

namespace tpc_qt::view_models {

class MainViewModel : public QObject {
    Q_OBJECT

    // Свойства под-вьюмоделей
    Q_PROPERTY(WorkspaceViewModel* workspace READ workspace CONSTANT)
    Q_PROPERTY(BottomBarViewModel* bottomBar READ bottom_bar CONSTANT)
    Q_PROPERTY(SettingsViewModel* settings READ settings CONSTANT)

    // Единое свойство состояния навигации
    Q_PROPERTY(int current_tab_index READ current_tab_index NOTIFY current_tab_index_changed)

public:
    explicit MainViewModel(QObject *parent = nullptr)
        : QObject(parent)
        , workspace_(new WorkspaceViewModel(this))
        , bottom_bar_(new BottomBarViewModel(this, models::ui::TabsIndexingModel::Workspace))
        , settings_(new SettingsViewModel(this))
    {
        // Подписка на глобальные события навигации
        services::EventDispatcher::instance().tab_change_requested.subscribe(
            [this](models::ui::TabsIndexingModel target_tab) {
                on_tab_change_requested(target_tab);
            }
        );
    }

    WorkspaceViewModel* workspace() const noexcept { return workspace_; }
    BottomBarViewModel* bottom_bar() const noexcept { return bottom_bar_; }
    SettingsViewModel* settings() const noexcept { return settings_; }

    // Возвращаем enum, скастованный в int для QML
    int current_tab_index() const noexcept {
        return static_cast<int>(current_tab_);
    }

signals:
    void current_tab_index_changed();

private:
    void on_tab_change_requested(models::ui::TabsIndexingModel target_tab) {
        if (current_tab_ == target_tab) return; // Защита от лишних обновлений UI

        current_tab_ = target_tab;
        emit current_tab_index_changed();
    }

private:
    WorkspaceViewModel* workspace_;
    BottomBarViewModel* bottom_bar_;
    SettingsViewModel* settings_;

    // Текущее состояние приложения
    models::ui::TabsIndexingModel current_tab_{models::ui::TabsIndexingModel::Workspace};
};

} // namespace tpc_qt::view_models