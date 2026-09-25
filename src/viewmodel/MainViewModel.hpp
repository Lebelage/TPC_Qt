#pragma once

#include <QObject>

#include "controls/BottomBarViewModel.hpp"
#include "controls/SettingsTabViewModel.hpp"
#include "controls/WorkspaceTabViewModel.hpp"
#include "FieldVisualizationViewModel.hpp"
#include "models/ui/tabs_indexing_model.hpp"
#include "services/scoped_subscription.hpp"

namespace tpc_qt::services {
class EventDispatcher;
class SettingsHolderService;
class TpcService;
class FieldSliceService;
}

namespace tpc_qt::view_models {

/** Root QML context object and owner of the screen-level view models. */
class MainViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(WorkspaceViewModel* workspace READ workspace CONSTANT)
    Q_PROPERTY(BottomBarViewModel* bottomBar READ bottomBar CONSTANT)
    Q_PROPERTY(SettingsViewModel* settings READ settings CONSTANT)
    Q_PROPERTY(FieldVisualizationViewModel* fieldVisualization READ fieldVisualization CONSTANT)
    Q_PROPERTY(int currentTabIndex READ currentTabIndex NOTIFY currentTabIndexChanged)

public:
    MainViewModel(
        services::EventDispatcher& events,
        services::SettingsHolderService& settings,
        services::TpcService& tpc,
        services::FieldSliceService& field_slices,
        QObject* parent = nullptr
    );

    [[nodiscard]] WorkspaceViewModel* workspace() noexcept { return &workspace_; }
    [[nodiscard]] BottomBarViewModel* bottomBar() noexcept { return &bottom_bar_; }
    [[nodiscard]] SettingsViewModel* settings() noexcept { return &settings_; }
    [[nodiscard]] FieldVisualizationViewModel* fieldVisualization() noexcept { return &field_visualization_; }
    [[nodiscard]] int currentTabIndex() const noexcept { return static_cast<int>(current_tab_); }

Q_SIGNALS:
    void currentTabIndexChanged();

private:
    void onTabChangeRequested(models::ui::TabsIndexingModel target_tab);

    WorkspaceViewModel workspace_;
    BottomBarViewModel bottom_bar_;
    SettingsViewModel settings_;
    FieldVisualizationViewModel field_visualization_;
    models::ui::TabsIndexingModel current_tab_{models::ui::TabsIndexingModel::Workspace};
    services::ScopedSubscription<models::ui::TabsIndexingModel> tab_subscription_;
};

}  // namespace tpc_qt::view_models
