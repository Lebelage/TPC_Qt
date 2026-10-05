#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include <slint.h>

#include "app.h"
#include "models/application_settings_model.hpp"
#include "models/field_slice_model.hpp"
#include "models/tpc_data_model.hpp"
#include "services/scoped_subscription.hpp"

namespace tpc_slint::application {
class ApplicationContext;
}

namespace tpc_slint::services {
struct RenderedFieldSlice;
}

namespace tpc_slint::ui {

/** Connects generated Slint components to toolkit-independent services. */
class ApplicationPresenter final {
public:
    ApplicationPresenter(
        application::ApplicationContext& context,
        slint::ComponentHandle<MainWindow> main_window,
        slint::ComponentHandle<FieldWindow> field_window
    );
    ~ApplicationPresenter();

    ApplicationPresenter(const ApplicationPresenter&) = delete;
    ApplicationPresenter& operator=(const ApplicationPresenter&) = delete;

    void showStartupError(std::string_view message);

private:
    void bindServiceEvents();
    void bindUiCallbacks();
    void applySettings(const models::AppSettings& settings);
    void mergeSensors(const models::AppSettings& settings);
    void applyFrame(const models::TpcDataModel::ReceivedFrame& frame);
    void updateSensorRows();
    void acceptGeometry(models::FieldGeometry geometry);
    void acceptSlice(std::shared_ptr<const services::RenderedFieldSlice> slice);
    void calculateField();
    void selectAxis(int axis);
    void selectPosition(float position);
    void selectPosition(std::string_view position);
    void requestSlice();
    void setStatus(std::string_view message, int level = 0);
    [[nodiscard]] double axisExtent() const noexcept;

    application::ApplicationContext& context_;
    slint::ComponentHandle<MainWindow> main_window_;
    slint::ComponentHandle<FieldWindow> field_window_;
    std::vector<models::Sensor> sensors_;
    models::FieldGeometry geometry_;
    std::shared_ptr<const services::RenderedFieldSlice> rendered_slice_;
    int axis_{2};
    double position_{};
    int viewport_width_{640};
    int viewport_height_{520};

    services::ScopedSubscription<const models::AppSettings&> settings_subscription_;
    services::ScopedSubscription<const models::TpcDataModel::ReceivedFrame&> frame_subscription_;
    services::ScopedSubscription<bool> connection_subscription_;
    services::ScopedSubscription<bool> calculation_subscription_;
    services::ScopedSubscription<models::FieldGeometry> geometry_subscription_;
    services::ScopedSubscription<std::shared_ptr<const services::RenderedFieldSlice>> slice_subscription_;
};

}  // namespace tpc_slint::ui
