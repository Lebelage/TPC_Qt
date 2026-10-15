#include "ui/application_view_binding.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "services/field_slice/rendered_field_slice.hpp"
#include "ui/file_dialog.hpp"
#include "viewmodels/application_view_model.hpp"
#include "viewmodels/field_map_overlay.hpp"

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
    row.enabled = source.enabled;
    row.selectable = source.selectable;
    row.status = toSharedString(source.status);
    return row;
}

[[nodiscard]] std::shared_ptr<slint::VectorModel<MapTick>> toTickModel(std::span<const viewmodels::FieldTick> ticks) {
    std::vector<MapTick> rows;
    rows.reserve(ticks.size());
    for (const auto& tick : ticks) {
        MapTick row;
        row.position = static_cast<float>(tick.position);
        row.label = toSharedString(tick.label);
        row.zero = tick.zero;
        row.major = tick.major;
        rows.push_back(std::move(row));
    }
    return std::make_shared<slint::VectorModel<MapTick>>(std::move(rows));
}

}  // namespace

ApplicationViewBinding::ApplicationViewBinding(
    viewmodels::ApplicationViewModel& view_model,
    slint::ComponentHandle<MainWindow> main_window,
    slint::ComponentHandle<FieldWindow> field_window
)
    : view_model_(view_model), main_window_(std::move(main_window)), field_window_(std::move(field_window)) {
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
        dispatchToUi([this, state = std::move(state)] {
            renderMain(state);
        });
    });
    settings_state_subscription_.subscribe(view_model_.settings_state_changed, [this] {
        auto state = view_model_.mainState();
        dispatchToUi([this, state = std::move(state)] {
            renderSettings(state);
        });
    });
    field_state_subscription_.subscribe(view_model_.field_state_changed, [this] {
        auto state = view_model_.fieldState();
        dispatchToUi([this, state = std::move(state)] {
            renderField(state);
        });
    });
}

void ApplicationViewBinding::bindUiCallbacks() {
    main_window_->on_connect_requested([this] {
        view_model_.connect();
    });
    main_window_->on_disconnect_requested([this] {
        view_model_.disconnect();
    });
    main_window_->on_calculate_field([this] {
        view_model_.calculateField();
    });
    main_window_->on_visualize_field([this] {
        field_window_->show();
        field_window_->invoke_refresh_plot();
    });
    main_window_->on_save_field([this] {
        if (const auto path = showVtkSaveDialog()) {
            view_model_.exportField(path->string());
        }
    });
    main_window_->on_load_settings([this] {
        view_model_.reloadSettings();
    });
    main_window_->on_browse_settings([this] {
        if (const auto path = showSettingsOpenDialog())
            view_model_.loadSettings(path->string());
    });
    main_window_->on_load_settings_file([this](const slint::SharedString& path) {
        view_model_.loadSettings(toString(path));
    });
    main_window_->on_sensor_toggled([this](const slint::SharedString& name, bool enabled) {
        view_model_.setSensorEnabled(toString(name), enabled);
    });
    main_window_->on_apply_settings([this](
                                        const slint::SharedString& length,
                                        const slint::SharedString& radius,
                                        const slint::SharedString& endpoint,
                                        const slint::SharedString& polling,
                                        const slint::SharedString& nx,
                                        const slint::SharedString& ny,
                                        const slint::SharedString& nz
                                    ) {
        view_model_.applySettings(
            {.length = toString(length),
             .radius = toString(radius),
             .endpoint = toString(endpoint),
             .polling_interval = toString(polling),
             .grid_nx = toString(nx),
             .grid_ny = toString(ny),
             .grid_nz = toString(nz)}
        );
    });

    field_window_->on_axis_changed([this](int axis) {
        view_model_.selectAxis(axis);
    });
    field_window_->on_position_changed([this](float position) {
        view_model_.selectPosition(position);
    });
    field_window_->on_position_entered([this](const slint::SharedString& position) {
        view_model_.selectPosition(std::string_view{position.data(), position.size()});
    });
    field_window_->on_calculate_field([this] {
        view_model_.calculateField();
    });
    field_window_->on_viewport_changed([this](float width, float height) {
        view_model_.setViewport(width, height);
    });
    field_window_->on_map_view_changed(
        [this](float width, float height, float x, float y, float map_width, float map_height) {
            const viewmodels::FieldViewport next{width, height, x, y, map_width, map_height};
            if (next == field_viewport_)
                return;
            field_viewport_ = next;
            renderGrid();
        }
    );
    field_window_->on_inspect_field([this](float x, float y, float width, float height) {
        return toSharedString(view_model_.inspectField(x, y, width, height, rendered_slice_));
    });
}

void ApplicationViewBinding::renderMain(const viewmodels::MainWindowViewState& state) {
    field_window_->set_field_unit(state.scientific_status.field_message.starts_with("PRELIMINARY") ? "G/mV" : "G");
    if (!state.field_calculated)
        field_window_->set_probe_text(slint::SharedString{});
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
    main_window_->set_enabled_sensor_count(static_cast<int>(state.enabled_sensor_count));
    main_window_->set_data_quality(toSharedString(state.scientific_status.data_message));
    main_window_->set_field_quality(toSharedString(state.scientific_status.field_message));
    main_window_->set_homogeneous(state.scientific_status.homogeneous);
    main_window_->set_status_message(toSharedString(state.status_message));
    main_window_->set_status_level(state.status_level);
}

void ApplicationViewBinding::renderSettings(const viewmodels::MainWindowViewState& state) {
    main_window_->set_settings_path(toSharedString(state.settings_path));
    main_window_->set_active_settings_path(toSharedString(state.settings_path));
    main_window_->set_settings_file_name(
        toSharedString(std::filesystem::path(state.settings_path).filename().string())
    );
    main_window_->set_tpc_length(toSharedString(state.tpc_length));
    main_window_->set_tpc_radius(toSharedString(state.tpc_radius));
    main_window_->set_endpoint(toSharedString(state.endpoint));
    main_window_->set_polling_interval(toSharedString(state.polling_interval));
    main_window_->set_grid_nx(toSharedString(state.grid_nx));
    main_window_->set_grid_ny(toSharedString(state.grid_ny));
    main_window_->set_grid_nz(toSharedString(state.grid_nz));
}

void ApplicationViewBinding::renderField(const viewmodels::FieldWindowViewState& state) {
    field_window_->set_probe_text(slint::SharedString{});
    field_window_->set_available(state.available);
    field_window_->set_loading(state.loading);
    field_window_->set_axis_index(state.axis_index);
    field_window_->set_slice_position(state.slice_position);
    field_window_->set_position_input(toSharedString(state.position_input));
    field_window_->set_minimum_position(state.minimum_position);
    field_window_->set_maximum_position(state.maximum_position);
    field_window_->set_position_label(toSharedString(state.position_label));

    const bool slice_changed = state.rendered_slice && rendered_slice_ != state.rendered_slice;
    const bool sensors_changed = rendered_sensors_ != state.sensors;
    rendered_sensors_ = state.sensors;
    if (slice_changed) {
        rendered_slice_ = state.rendered_slice;
        renderSlice();
        field_window_->invoke_refresh_plot();
        renderGrid();
    }
    if (slice_changed || sensors_changed) {
        renderSensors();
    }
}

void ApplicationViewBinding::renderSlice() {
    const auto& slice = *rendered_slice_;
    slint::SharedPixelBuffer<slint::Rgba8Pixel> buffer(
        static_cast<std::uint32_t>(slice.displayWidth()),
        static_cast<std::uint32_t>(slice.displayHeight()),
        reinterpret_cast<const slint::Rgba8Pixel*>(slice.rgba.data())
    );
    field_window_->set_slice_image(slint::Image{buffer});
    field_window_->set_image_width(static_cast<float>(slice.displayWidth()));
    field_window_->set_image_height(static_cast<float>(slice.displayHeight()));
    field_window_->set_horizontal_minimum(static_cast<float>(slice.horizontalRange()[0]));
    field_window_->set_horizontal_maximum(static_cast<float>(slice.horizontalRange()[1]));
    field_window_->set_vertical_minimum(static_cast<float>(slice.verticalRange()[0]));
    field_window_->set_vertical_maximum(static_cast<float>(slice.verticalRange()[1]));
    field_window_->set_displayed_axis(slice.axis);
    field_window_->set_color_ticks(toTickModel(viewmodels::fieldColorTicks(slice.minimum_value, slice.maximum_value)));
}

void ApplicationViewBinding::renderGrid() {
    if (!rendered_slice_)
        return;
    const auto grid = viewmodels::fieldGrid(*rendered_slice_, field_viewport_);
    field_window_->set_horizontal_ticks(toTickModel(grid.horizontal));
    field_window_->set_vertical_ticks(toTickModel(grid.vertical));
    field_window_->set_grid_step(static_cast<float>(grid.major_step));
}

void ApplicationViewBinding::renderSensors() {
    if (!rendered_slice_)
        return;
    std::vector<MapSensor> markers;
    if (rendered_sensors_) {
        for (const auto& marker : viewmodels::fieldSensorMarkers(*rendered_slice_, *rendered_sensors_)) {
            MapSensor row;
            row.u = static_cast<float>(marker.u);
            row.v = static_cast<float>(marker.v);
            row.in_slice = marker.in_slice;
            markers.push_back(std::move(row));
        }
    }
    field_window_->set_map_sensors(std::make_shared<slint::VectorModel<MapSensor>>(std::move(markers)));
}

}  // namespace tpc_slint::ui
