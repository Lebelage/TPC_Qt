#pragma once

#include <memory>
#include <vector>
#include <slint.h>

#include "app.h"
#include "services/scoped_subscription.hpp"

namespace tpc_slint::viewmodels {
class ApplicationViewModel;
struct MainWindowViewState;
struct FieldWindowViewState;
struct SensorRowViewState;
}

namespace tpc_slint::services {
struct RenderedFieldSlice;
}

namespace tpc_slint::ui {

/** Thin Slint View binding for the toolkit-independent application ViewModel. */
class ApplicationViewBinding final {
public:
    ApplicationViewBinding(
        viewmodels::ApplicationViewModel& view_model,
        slint::ComponentHandle<MainWindow> main_window,
        slint::ComponentHandle<FieldWindow> field_window
    );

    ApplicationViewBinding(const ApplicationViewBinding&) = delete;
    ApplicationViewBinding& operator=(const ApplicationViewBinding&) = delete;

private:
    void bindViewModel();
    void bindUiCallbacks();
    void renderMain(const viewmodels::MainWindowViewState& state);
    void renderSettings(const viewmodels::MainWindowViewState& state);
    void renderField(const viewmodels::FieldWindowViewState& state);

    viewmodels::ApplicationViewModel& view_model_;
    slint::ComponentHandle<MainWindow> main_window_;
    slint::ComponentHandle<FieldWindow> field_window_;
    std::shared_ptr<slint::VectorModel<SensorRow>> sensor_model_;
    std::shared_ptr<const std::vector<viewmodels::SensorRowViewState>> rendered_rows_;
    std::shared_ptr<const services::RenderedFieldSlice> rendered_slice_;
    services::ScopedSubscription<> main_state_subscription_;
    services::ScopedSubscription<> settings_state_subscription_;
    services::ScopedSubscription<> field_state_subscription_;
};

}  // namespace tpc_slint::ui
