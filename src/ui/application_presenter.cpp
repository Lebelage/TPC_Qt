#include "ui/application_presenter.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "application/application_context.hpp"
#include "services/field_slice/field_slice_service.hpp"

namespace tpc_slint::ui {
namespace {
namespace fs = std::filesystem;

template <class Function>
void dispatchToUi(Function&& function) {
    (void)slint::invoke_from_event_loop(std::forward<Function>(function));
}

[[nodiscard]] std::optional<double> parseDouble(const slint::SharedString& text) {
    const std::string value{text};
    char* end = nullptr;
    const double parsed = std::strtod(value.c_str(), &end);
    if (end != value.c_str() + value.size() || !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] std::optional<int> parseInt(const slint::SharedString& text) {
    const std::string value{text};
    int parsed{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size()) {
        return std::nullopt;
    }
    return parsed;
}

}  // namespace

ApplicationPresenter::ApplicationPresenter(
    application::ApplicationContext& context,
    slint::ComponentHandle<MainWindow> main_window,
    slint::ComponentHandle<FieldWindow> field_window
)
    : context_(context),
      main_window_(std::move(main_window)),
      field_window_(std::move(field_window)) {
    bindServiceEvents();
    bindUiCallbacks();
}

ApplicationPresenter::~ApplicationPresenter() {
    // Stop the only service that can still publish UI work after the event
    // loop exits. Subscription members are then released before the context.
    context_.fieldSlices().dispose();
}

void ApplicationPresenter::showStartupError(std::string_view message) {
    setStatus(message, 2);
}

void ApplicationPresenter::bindServiceEvents() {
    auto& events = context_.events();
    auto& field_slices = context_.fieldSlices();

    settings_subscription_.subscribe(events.settings_changed, [this](const models::AppSettings& settings) {
        const auto copy = settings;
        dispatchToUi([this, copy] { applySettings(copy); });
    });
    frame_subscription_.subscribe(events.frame_received, [this](const models::TpcDataModel::ReceivedFrame& frame) {
        const auto copy = frame;
        dispatchToUi([this, copy] {
            applyFrame(copy);
            updateSensorRows();
        });
    });
    connection_subscription_.subscribe(events.connection_state_changed, [this](bool connected) {
        dispatchToUi([this, connected] {
            main_window_->set_connected(connected);
            setStatus(connected ? "Connected to TPC" : "Disconnected from TPC", connected ? 1 : 0);
        });
    });
    calculation_subscription_.subscribe(events.field_was_calculated_, [this](bool calculated) {
        dispatchToUi([this, calculated] {
            main_window_->set_field_calculated(calculated);
            setStatus(calculated ? "Field calculation completed" : "Field calculation failed", calculated ? 1 : 2);
        });
    });
    geometry_subscription_.subscribe(field_slices.field_available, [this](models::FieldGeometry geometry) {
        dispatchToUi([this, geometry] { acceptGeometry(geometry); });
    });
    slice_subscription_.subscribe(
        field_slices.slice_rendered,
        [this](std::shared_ptr<const services::RenderedFieldSlice> slice) {
            dispatchToUi([this, slice = std::move(slice)] { acceptSlice(std::move(slice)); });
        }
    );
}

void ApplicationPresenter::bindUiCallbacks() {
    main_window_->on_connect_requested([this] {
        const auto current = context_.settings().currentSettings();
        setStatus("Connecting to TPC…");
        if (!context_.tpc().connectAsync(current.connection.endpoint)) {
            setStatus("Could not initialize the TPC connection", 2);
        }
    });
    main_window_->on_disconnect_requested([this] { context_.tpc().disconnect(); });
    main_window_->on_calculate_field([this] {
        main_window_->set_field_calculated(false);
        setStatus("Calculating field…");
        context_.tpc().calculateField();
    });
    main_window_->on_visualize_field([this] { field_window_->show(); });
    main_window_->on_save_field([this](const slint::SharedString& path_text) {
        fs::path path{std::string{path_text}};
        if (path.empty()) {
            setStatus("Enter a VTK output path", 2);
            return;
        }
        if (!path.has_extension()) {
            path.replace_extension(".vtk");
            main_window_->set_export_path(slint::SharedString{path.string()});
        }
        const bool saved = context_.tpc().exportFieldToVtk(path.string());
        setStatus(saved ? "VTK field exported successfully" : "Could not export the VTK field", saved ? 1 : 2);
    });
    main_window_->on_load_settings([this] {
        const auto loaded = context_.settings().loadSettings();
        setStatus(loaded ? "Settings reloaded" : loaded.error(), loaded ? 1 : 2);
    });
    main_window_->on_apply_settings([this](
        const slint::SharedString& length_text,
        const slint::SharedString& radius_text,
        const slint::SharedString& endpoint,
        const slint::SharedString& polling_text,
        const slint::SharedString& nx_text,
        const slint::SharedString& ny_text,
        const slint::SharedString& nz_text
    ) {
        const auto length = parseDouble(length_text);
        const auto radius = parseDouble(radius_text);
        const auto polling = parseInt(polling_text);
        const auto nx = parseInt(nx_text);
        const auto ny = parseInt(ny_text);
        const auto nz = parseInt(nz_text);
        if (!length || !radius || !polling || !nx || !ny || !nz
            || *length <= 0.0 || *radius <= 0.0 || *polling <= 0
            || *nx <= 0 || *ny <= 0 || *nz <= 0) {
            setStatus("Settings contain invalid values", 2);
            return;
        }

        auto& settings = context_.settings();
        settings.setConnectionParameters({std::string{endpoint}, *polling});
        settings.setGeometryParameters({*length, *radius});
        settings.setGridParameters({
            static_cast<std::size_t>(*nx),
            static_cast<std::size_t>(*ny),
            static_cast<std::size_t>(*nz)
        });
        settings.applySettings();
        setStatus("Settings applied and saved", 1);
    });

    field_window_->on_axis_changed([this](int axis) { selectAxis(axis); });
    field_window_->on_position_changed([this](float position) { selectPosition(position); });
    field_window_->on_viewport_changed([this](float width, float height) {
        viewport_width_ = std::clamp(static_cast<int>(width), 128, 1024);
        viewport_height_ = std::clamp(static_cast<int>(height), 128, 1024);
    });
}

void ApplicationPresenter::applySettings(const models::AppSettings& settings) {
    main_window_->set_tpc_length(slint::SharedString{std::format("{:g}", settings.geometry.length)});
    main_window_->set_tpc_radius(slint::SharedString{std::format("{:g}", settings.geometry.radius)});
    main_window_->set_endpoint(slint::SharedString{settings.connection.endpoint});
    main_window_->set_polling_interval(slint::SharedString{std::to_string(settings.connection.polling_interval)});
    main_window_->set_grid_nx(slint::SharedString{std::to_string(settings.grid[0])});
    main_window_->set_grid_ny(slint::SharedString{std::to_string(settings.grid[1])});
    main_window_->set_grid_nz(slint::SharedString{std::to_string(settings.grid[2])});
    mergeSensors(settings);
    updateSensorRows();
    main_window_->set_field_calculated(false);
}

void ApplicationPresenter::mergeSensors(const models::AppSettings& settings) {
    std::vector<models::Sensor> sensors;
    sensors.reserve(settings.sensors_info.size());
    for (const auto& info : settings.sensors_info) {
        models::Sensor sensor{.name = info.name, .position = {info.x, info.y, info.z}};
        const auto previous = std::ranges::find_if(sensors_, [&info](const models::Sensor& current) {
            return current.name.id == info.name.id && current.name.number == info.name.number;
        });
        if (previous != sensors_.end()) {
            sensor.values = previous->values;
        }
        sensors.push_back(sensor);
    }
    sensors_ = std::move(sensors);
}

void ApplicationPresenter::applyFrame(const models::TpcDataModel::ReceivedFrame& frame) {
    for (const auto& [channel, value] : frame) {
        const auto name = models::SensorName::parse(channel);
        const auto component = models::SensorName::parseComponent(channel);
        if (!name || !component) {
            continue;
        }
        const auto sensor = std::ranges::find_if(sensors_, [&name](const models::Sensor& current) {
            return current.name.id == name->id && current.name.number == name->number;
        });
        if (sensor != sensors_.end()) {
            sensor->setValue(value, *component);
        }
    }
}

void ApplicationPresenter::updateSensorRows() {
    std::vector<SensorRow> rows;
    rows.reserve(sensors_.size());
    for (const auto& sensor : sensors_) {
        SensorRow row;
        row.name = sensor.name.toString();
        row.radial = std::format("{:.3f}", sensor.values[0]);
        row.azimuthal = std::format("{:.3f}", sensor.values[1]);
        row.longitudinal = std::format("{:.3f}", sensor.values[2]);
        row.coordinate_x = std::format("{:.3f}", sensor.position[0]);
        row.coordinate_y = std::format("{:.3f}", sensor.position[1]);
        row.coordinate_z = std::format("{:.3f}", sensor.position[2]);
        rows.push_back(std::move(row));
    }
    main_window_->set_sensor_rows(std::make_shared<slint::VectorModel<SensorRow>>(std::move(rows)));
}

void ApplicationPresenter::acceptGeometry(models::FieldGeometry geometry) {
    geometry_ = geometry;
    position_ = 0.0;
    const double extent = axisExtent();
    field_window_->set_available(extent > 0.0);
    field_window_->set_minimum_position(static_cast<float>(-extent));
    field_window_->set_maximum_position(static_cast<float>(extent));
    field_window_->set_slice_position(0.0f);
    requestSlice();
}

void ApplicationPresenter::acceptSlice(std::shared_ptr<const services::RenderedFieldSlice> slice) {
    rendered_slice_ = std::move(slice);
    slint::SharedPixelBuffer<slint::Rgba8Pixel> buffer(
        static_cast<std::uint32_t>(rendered_slice_->width),
        static_cast<std::uint32_t>(rendered_slice_->height),
        reinterpret_cast<const slint::Rgba8Pixel*>(rendered_slice_->rgba.data())
    );
    field_window_->set_slice_image(slint::Image{buffer});
    field_window_->set_minimum_value(static_cast<float>(rendered_slice_->minimum_value));
    field_window_->set_maximum_value(static_cast<float>(rendered_slice_->maximum_value));
    field_window_->set_loading(false);
}

void ApplicationPresenter::selectAxis(int axis) {
    axis_ = std::clamp(axis, 0, 2);
    position_ = 0.0;
    const double extent = axisExtent();
    field_window_->set_minimum_position(static_cast<float>(-extent));
    field_window_->set_maximum_position(static_cast<float>(extent));
    field_window_->set_slice_position(0.0f);
    requestSlice();
}

void ApplicationPresenter::selectPosition(float position) {
    const double extent = axisExtent();
    const double bounded = std::clamp(static_cast<double>(position), -extent, extent);
    if (std::abs(bounded - position_) <= std::numeric_limits<double>::epsilon()) {
        return;
    }
    position_ = bounded;
    requestSlice();
}

void ApplicationPresenter::requestSlice() {
    if (geometry_.radius <= 0.0 || geometry_.length <= 0.0) {
        return;
    }
    field_window_->set_loading(true);
    static constexpr std::array<std::string_view, 3> axis_names{"X", "Y", "Z"};
    field_window_->set_position_label(slint::SharedString{std::format(
        "{} = {:.5g} cm", axis_names[static_cast<std::size_t>(axis_)], position_
    )});
    context_.fieldSlices().requestSlice(axis_, position_, viewport_width_, viewport_height_);
}

void ApplicationPresenter::setStatus(std::string_view message, int level) {
    main_window_->set_status_message(slint::SharedString{message});
    main_window_->set_status_level(std::clamp(level, 0, 2));
}

double ApplicationPresenter::axisExtent() const noexcept {
    return axis_ == 2 ? geometry_.length * 0.5 : geometry_.radius;
}

}  // namespace tpc_slint::ui
