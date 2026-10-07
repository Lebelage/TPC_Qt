#include "ui/application_view_binding.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "services/field_slice/field_slice_service.hpp"
#include "ui/file_dialog.hpp"
#include "viewmodels/application_view_model.hpp"

namespace tpc_slint::ui {
namespace {

template <class Function>
void dispatchToUi(Function&& function) {
    (void)slint::invoke_from_event_loop(std::forward<Function>(function));
}

[[nodiscard]] std::string toString(const slint::SharedString& value) {
    return {value.data(), value.size()};
}

[[nodiscard]] slint::SharedString toSharedString(const std::string& value) {
    return slint::SharedString{std::string_view{value}};
}

[[nodiscard]] SensorRow toSensorRow(const viewmodels::SensorRowViewState& source) {
    SensorRow row;
    row.name = toSharedString(source.name);
    row.radial = toSharedString(source.radial);
    row.azimuthal = toSharedString(source.azimuthal);
    row.longitudinal = toSharedString(source.longitudinal);
    row.coordinate_x = toSharedString(source.coordinate_x);
    row.coordinate_y = toSharedString(source.coordinate_y);
    row.coordinate_z = toSharedString(source.coordinate_z);
    return row;
}

}  // namespace

ApplicationViewBinding::ApplicationViewBinding(
    viewmodels::ApplicationViewModel& view_model,
    slint::ComponentHandle<MainWindow> main_window,
    slint::ComponentHandle<FieldWindow> field_window
)
    : view_model_(view_model),
      main_window_(std::move(main_window)),
      field_window_(std::move(field_window)) {
    bindViewModel();
    bindUiCallbacks();
    const auto main_state = view_model_.mainState();
    renderMain(main_state);
    renderSettings(main_state);
    renderField(view_model_.fieldState());
}

void ApplicationViewBinding::bindViewModel() {
    main_state_subscription_.subscribe(view_model_.main_state_changed, [this] {
        auto state = view_model_.mainState();
        dispatchToUi([this, state = std::move(state)] { renderMain(state); });
    });
    settings_state_subscription_.subscribe(view_model_.settings_state_changed, [this] {
        auto state = view_model_.mainState();
        dispatchToUi([this, state = std::move(state)] { renderSettings(state); });
    });
    field_state_subscription_.subscribe(view_model_.field_state_changed, [this] {
        auto state = view_model_.fieldState();
        dispatchToUi([this, state = std::move(state)] { renderField(state); });
    });
}

void ApplicationViewBinding::bindUiCallbacks() {
    main_window_->on_connect_requested([this] { view_model_.connect(); });
    main_window_->on_disconnect_requested([this] { view_model_.disconnect(); });
    main_window_->on_calculate_field([this] { view_model_.calculateField(); });
    main_window_->on_visualize_field([this] { field_window_->show(); });
    main_window_->on_save_field([this] {
        if (const auto path = showVtkSaveDialog()) {
            view_model_.exportField(path->string());
        }
    });
    main_window_->on_load_settings([this] { view_model_.reloadSettings(); });
    main_window_->on_apply_settings([this](
        const slint::SharedString& length,
        const slint::SharedString& radius,
        const slint::SharedString& endpoint,
        const slint::SharedString& polling,
        const slint::SharedString& nx,
        const slint::SharedString& ny,
        const slint::SharedString& nz
    ) {
        view_model_.applySettings({
            .length = toString(length),
            .radius = toString(radius),
            .endpoint = toString(endpoint),
            .polling_interval = toString(polling),
            .grid_nx = toString(nx),
            .grid_ny = toString(ny),
            .grid_nz = toString(nz)
        });
    });

    field_window_->on_axis_changed([this](int axis) { view_model_.selectAxis(axis); });
    field_window_->on_position_changed([this](float position) { view_model_.selectPosition(position); });
    field_window_->on_position_entered([this](const slint::SharedString& position) {
        view_model_.selectPosition(std::string_view{position.data(), position.size()});
    });
    field_window_->on_calculate_field([this] { view_model_.calculateField(); });
    field_window_->on_viewport_changed([this](float width, float height) {
        view_model_.setViewport(width, height);
    });
}

void ApplicationViewBinding::renderMain(const viewmodels::MainWindowViewState& state) {
    if (rendered_rows_ != state.sensor_rows) {
        const auto& source = *state.sensor_rows;
        if (!sensor_model_ || !rendered_rows_ || rendered_rows_->size() != source.size()) {
            std::vector<SensorRow> rows;
            rows.reserve(source.size());
            for (const auto& row : source) {
                rows.push_back(toSensorRow(row));
            }
            sensor_model_ = std::make_shared<slint::VectorModel<SensorRow>>(std::move(rows));
            main_window_->set_sensor_rows(sensor_model_);
        } else {
            for (std::size_t index = 0; index < source.size(); ++index) {
                if ((*rendered_rows_)[index] != source[index]) {
                    sensor_model_->set_row_data(index, toSensorRow(source[index]));
                }
            }
        }
        rendered_rows_ = state.sensor_rows;
    }
    main_window_->set_connected(state.connected);
    main_window_->set_field_calculated(state.field_calculated);
    main_window_->set_calculating(state.calculating);
    main_window_->set_data_quality(toSharedString(state.scientific_status.data_message));
    main_window_->set_field_quality(toSharedString(state.scientific_status.field_message));
    main_window_->set_homogeneous(state.scientific_status.homogeneous);
    main_window_->set_status_message(toSharedString(state.status_message));
    main_window_->set_status_level(state.status_level);
}

void ApplicationViewBinding::renderSettings(const viewmodels::MainWindowViewState& state) {
    main_window_->set_tpc_length(toSharedString(state.tpc_length));
    main_window_->set_tpc_radius(toSharedString(state.tpc_radius));
    main_window_->set_endpoint(toSharedString(state.endpoint));
    main_window_->set_polling_interval(toSharedString(state.polling_interval));
    main_window_->set_grid_nx(toSharedString(state.grid_nx));
    main_window_->set_grid_ny(toSharedString(state.grid_ny));
    main_window_->set_grid_nz(toSharedString(state.grid_nz));
}

void ApplicationViewBinding::renderField(const viewmodels::FieldWindowViewState& state) {
    field_window_->set_available(state.available);
    field_window_->set_loading(state.loading);
    field_window_->set_axis_index(state.axis_index);
    field_window_->set_slice_position(state.slice_position);
    field_window_->set_position_input(toSharedString(state.position_input));
    field_window_->set_minimum_position(state.minimum_position);
    field_window_->set_maximum_position(state.maximum_position);
    field_window_->set_minimum_value(state.minimum_value);
    field_window_->set_maximum_value(state.maximum_value);
    field_window_->set_position_label(toSharedString(state.position_label));

    if (state.rendered_slice && rendered_slice_ != state.rendered_slice) {
        const auto& slice = *state.rendered_slice;
        slint::SharedPixelBuffer<slint::Rgba8Pixel> buffer(
            static_cast<std::uint32_t>(slice.width),
            static_cast<std::uint32_t>(slice.height),
            reinterpret_cast<const slint::Rgba8Pixel*>(slice.rgba.data())
        );
        field_window_->set_slice_image(slint::Image{buffer});
        rendered_slice_ = state.rendered_slice;
    }
}

}  // namespace tpc_slint::ui
