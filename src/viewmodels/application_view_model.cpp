#include "viewmodels/application_view_model.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <format>
#include <optional>
#include <system_error>
#include <utility>

#include "application/application_context.hpp"
#include "models/application_settings_model.hpp"
#include "services/field_slice/field_slice_service.hpp"

namespace tpc_slint::viewmodels {
namespace {

[[nodiscard]] std::optional<double> parseDouble(std::string_view text) {
    // Preserve leading whitespace and '+' accepted by the settings editor.
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
        if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
            return std::nullopt;
        }
    }
    if (text.empty()) {
        return std::nullopt;
    }
    double parsed{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (error != std::errc{} || end != text.data() + text.size() || !std::isfinite(parsed)) {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] std::optional<int> parseInt(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    int parsed{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] std::uint32_t sensorKey(models::SensorName name) noexcept {
    return (static_cast<std::uint32_t>(name.id) << 16) | name.number;
}

template <class Row>
[[nodiscard]] auto& componentText(Row& row, models::SensorNameComponent component) noexcept {
    switch (component) {
        case models::SensorNameComponent::R: return row.radial;
        case models::SensorNameComponent::F: return row.azimuthal;
        case models::SensorNameComponent::Z: return row.longitudinal;
    }
    std::unreachable();
}

}  // namespace

ApplicationViewModel::ApplicationViewModel(application::ApplicationContext& context) : context_(context) {
    bindServiceEvents();
}

ApplicationViewModel::~ApplicationViewModel() {
    // Prevent workers from publishing while subscriptions are being
    // dismantled. ApplicationContext performs both operations defensively.
    context_.fieldSlices().dispose();
    context_.tpc().dispose();
}

MainWindowViewState ApplicationViewModel::mainState() const {
    std::scoped_lock lock{mutex_};
    return main_state_;
}

FieldWindowViewState ApplicationViewModel::fieldState() const {
    std::scoped_lock lock{mutex_};
    return field_state_;
}

void ApplicationViewModel::showStartupError(std::string_view message) {
    setStatus(message, 2);
}

void ApplicationViewModel::connect() {
    std::string endpoint;
    {
        std::scoped_lock lock{mutex_};
        endpoint = main_state_.endpoint;
        main_state_.status_message = "Connecting to TPC…";
        main_state_.status_level = 0;
    }
    main_state_changed.invoke();
    if (!context_.tpc().connectAsync(std::move(endpoint))) {
        setStatus("Could not initialize the TPC connection", 2);
    }
}

void ApplicationViewModel::disconnect() {
    context_.tpc().disconnect();
}

void ApplicationViewModel::calculateField() {
    {
        std::scoped_lock lock{mutex_};
        main_state_.field_calculated = false;
        main_state_.status_message = "Calculating field…";
        main_state_.status_level = 0;
        field_state_.available = false;
        field_state_.loading = true;
    }
    main_state_changed.invoke();
    field_state_changed.invoke();
    context_.tpc().calculateField();
}

void ApplicationViewModel::reloadSettings() {
    const auto loaded = context_.settings().loadSettings();
    setStatus(loaded ? "Settings reloaded" : loaded.error(), loaded ? 1 : 2);
}

void ApplicationViewModel::applySettings(SettingsInput input) {
    const auto length = parseDouble(input.length);
    const auto radius = parseDouble(input.radius);
    const auto polling = parseInt(input.polling_interval);
    const auto nx = parseInt(input.grid_nx);
    const auto ny = parseInt(input.grid_ny);
    const auto nz = parseInt(input.grid_nz);
    if (!length || !radius || !polling || !nx || !ny || !nz
        || *length <= 0.0 || *radius <= 0.0 || *polling <= 0
        || *nx <= 0 || *ny <= 0 || *nz <= 0) {
        setStatus("Settings contain invalid values", 2);
        return;
    }

    auto& settings = context_.settings();
    settings.setConnectionParameters({std::move(input.endpoint), *polling});
    settings.setGeometryParameters({*length, *radius});
    settings.setGridParameters({
        static_cast<std::size_t>(*nx),
        static_cast<std::size_t>(*ny),
        static_cast<std::size_t>(*nz)
    });
    settings.applySettings();
    setStatus("Settings applied and saved", 1);
}

void ApplicationViewModel::exportField(std::string_view path_text) {
    std::filesystem::path path{path_text};
    if (!path.has_extension()) {
        path.replace_extension(".vtk");
    }
    const bool saved = context_.tpc().exportFieldToVtk(path.string());
    setStatus(saved ? "VTK field exported successfully" : "Could not export the VTK field", saved ? 1 : 2);
}

void ApplicationViewModel::selectAxis(int axis) {
    {
        std::scoped_lock lock{mutex_};
        axis = std::clamp(axis, 0, 2);
        if (field_state_.axis_index == axis) {
            return;
        }
        field_state_.axis_index = axis;
        resetSlicePosition();
    }
    if (!requestSlice()) {
        field_state_changed.invoke();
    }
}

void ApplicationViewModel::selectPosition(float position) {
    selectPositionValue(position);
}

void ApplicationViewModel::selectPositionValue(double position) {
    if (!std::isfinite(position)) {
        return;
    }
    bool should_request = false;
    {
        std::scoped_lock lock{mutex_};
        const double extent = axisExtent();
        const double bounded = std::clamp(position, -extent, extent);
        should_request = bounded != position_;
        position_ = bounded;
        field_state_.slice_position = static_cast<float>(bounded);
        field_state_.position_input = std::format("{:.5g}", bounded);
    }
    if (!should_request || !requestSlice()) {
        field_state_changed.invoke();
    }
}

void ApplicationViewModel::selectPosition(std::string_view position) {
    const auto parsed = parseDouble(position);
    if (!parsed) {
        {
            std::scoped_lock lock{mutex_};
            field_state_.position_input = std::format("{:.5g}", position_);
        }
        field_state_changed.invoke();
        return;
    }
    selectPositionValue(*parsed);
}

void ApplicationViewModel::setViewport(float width, float height) {
    if (!std::isfinite(width) || !std::isfinite(height)) {
        return;
    }
    std::scoped_lock lock{mutex_};
    viewport_width_ = static_cast<int>(std::clamp(width, 128.0f, 1024.0f));
    viewport_height_ = static_cast<int>(std::clamp(height, 128.0f, 1024.0f));
}

void ApplicationViewModel::bindServiceEvents() {
    auto& events = context_.events();
    auto& field_slices = context_.fieldSlices();
    settings_subscription_.subscribe(events.settings_changed, [this](const models::AppSettings& settings) {
        acceptSettings(settings);
    });
    frame_subscription_.subscribe(events.frame_received, [this](const models::TpcDataModel::ReceivedFrame& frame) {
        acceptFrame(frame);
    });
    connection_subscription_.subscribe(events.connection_state_changed, [this](bool connected) {
        {
            std::scoped_lock lock{mutex_};
            main_state_.connected = connected;
            main_state_.status_message = connected ? "Connected to TPC" : "Disconnected from TPC";
            main_state_.status_level = connected ? 1 : 0;
        }
        main_state_changed.invoke();
    });
    calculation_subscription_.subscribe(events.field_was_calculated_, [this](bool calculated) {
        {
            std::scoped_lock lock{mutex_};
            main_state_.field_calculated = calculated;
            main_state_.status_message = calculated ? "Field calculation completed" : "Field calculation failed";
            main_state_.status_level = calculated ? 1 : 2;
            if (!calculated) {
                field_state_.loading = false;
            }
        }
        main_state_changed.invoke();
        if (!calculated) {
            field_state_changed.invoke();
        }
    });
    geometry_subscription_.subscribe(field_slices.field_available, [this](models::FieldGeometry geometry) {
        acceptGeometry(geometry);
    });
    slice_subscription_.subscribe(
        field_slices.slice_rendered,
        [this](std::shared_ptr<const services::RenderedFieldSlice> slice) { acceptSlice(std::move(slice)); }
    );
}

void ApplicationViewModel::acceptSettings(const models::AppSettings& settings) {
    {
        std::scoped_lock lock{mutex_};
        main_state_.tpc_length = std::format("{:g}", settings.geometry.length);
        main_state_.tpc_radius = std::format("{:g}", settings.geometry.radius);
        main_state_.endpoint = settings.connection.endpoint;
        main_state_.polling_interval = std::to_string(settings.connection.polling_interval);
        main_state_.grid_nx = std::to_string(settings.grid[0]);
        main_state_.grid_ny = std::to_string(settings.grid[1]);
        main_state_.grid_nz = std::to_string(settings.grid[2]);
        main_state_.field_calculated = false;

        std::vector<models::Sensor> sensors;
        sensors.reserve(settings.sensors_info.size());
        for (const auto& info : settings.sensors_info) {
            models::Sensor sensor{.name = info.name, .position = {info.x, info.y, info.z}};
            const auto previous = sensor_indices_.find(sensorKey(info.name));
            if (previous != sensor_indices_.end()) {
                sensor.values = sensors_[previous->second].values;
            }
            sensors.push_back(sensor);
        }
        sensors_ = std::move(sensors);
        sensor_indices_.clear();
        sensor_indices_.reserve(sensors_.size());
        for (std::size_t index = 0; index < sensors_.size(); ++index) {
            sensor_indices_.try_emplace(sensorKey(sensors_[index].name), index);
        }
        rebuildSensorRows();
    }
    settings_state_changed.invoke();
    main_state_changed.invoke();
}

void ApplicationViewModel::acceptFrame(const models::TpcDataModel::ReceivedFrame& frame) {
    bool changed = false;
    {
        std::scoped_lock lock{mutex_};
        std::shared_ptr<std::vector<SensorRowViewState>> rows;
        for (const auto& [channel, value] : frame) {
            const auto name = models::SensorName::parse(channel);
            const auto component = models::SensorName::parseComponent(channel);
            if (!name || !component) {
                continue;
            }
            const auto found = sensor_indices_.find(sensorKey(*name));
            if (found == sensor_indices_.end()) {
                continue;
            }
            const auto index = found->second;
            auto& sensor = sensors_[index];
            const auto component_index = static_cast<std::size_t>(*component);
            if (sensor.values[component_index] == value) {
                continue;
            }
            sensor.setValue(value, *component);
            auto formatted = std::format("{:.3f}", value);
            const auto& current = rows ? (*rows)[index] : (*main_state_.sensor_rows)[index];
            if (componentText(current, *component) == formatted) {
                continue;
            }
            if (!rows) {
                rows = std::make_shared<std::vector<SensorRowViewState>>(*main_state_.sensor_rows);
            }
            componentText((*rows)[index], *component) = std::move(formatted);
        }
        if (rows) {
            main_state_.sensor_rows = std::move(rows);
            changed = true;
        }
    }
    if (changed) {
        main_state_changed.invoke();
    }
}

void ApplicationViewModel::acceptGeometry(models::FieldGeometry geometry) {
    {
        std::scoped_lock lock{mutex_};
        geometry_ = geometry;
        resetSlicePosition();
        field_state_.available = axisExtent() > 0.0;
    }
    if (!requestSlice()) {
        field_state_changed.invoke();
    }
}

void ApplicationViewModel::acceptSlice(std::shared_ptr<const services::RenderedFieldSlice> slice) {
    if (!slice) {
        return;
    }
    {
        std::scoped_lock lock{mutex_};
        field_state_.rendered_slice = std::move(slice);
        field_state_.minimum_value = static_cast<float>(field_state_.rendered_slice->minimum_value);
        field_state_.maximum_value = static_cast<float>(field_state_.rendered_slice->maximum_value);
        field_state_.loading = false;
    }
    field_state_changed.invoke();
}

void ApplicationViewModel::rebuildSensorRows() {
    auto rows = std::make_shared<std::vector<SensorRowViewState>>();
    rows->reserve(sensors_.size());
    for (const auto& sensor : sensors_) {
        rows->push_back({
            .name = sensor.name.toString(),
            .radial = std::format("{:.3f}", sensor.values[0]),
            .azimuthal = std::format("{:.3f}", sensor.values[1]),
            .longitudinal = std::format("{:.3f}", sensor.values[2]),
            .coordinate_x = std::format("{:.3f}", sensor.position[0]),
            .coordinate_y = std::format("{:.3f}", sensor.position[1]),
            .coordinate_z = std::format("{:.3f}", sensor.position[2])
        });
    }
    main_state_.sensor_rows = std::move(rows);
}

void ApplicationViewModel::resetSlicePosition() {
    position_ = 0.0;
    const auto extent = axisExtent();
    field_state_.minimum_position = static_cast<float>(-extent);
    field_state_.maximum_position = static_cast<float>(extent);
    field_state_.slice_position = 0.0f;
    field_state_.position_input = "0";
}

bool ApplicationViewModel::requestSlice() {
    int axis = 2;
    double position = 0.0;
    int width = 640;
    int height = 520;
    {
        std::scoped_lock lock{mutex_};
        if (geometry_.radius <= 0.0 || geometry_.length <= 0.0) {
            return false;
        }
        field_state_.loading = true;
        static constexpr std::array<std::string_view, 3> axis_names{"X", "Y", "Z"};
        field_state_.position_label = std::format(
            "{} = {:.5g} cm", axis_names[static_cast<std::size_t>(field_state_.axis_index)], position_
        );
        axis = field_state_.axis_index;
        position = position_;
        width = viewport_width_;
        height = viewport_height_;
    }
    field_state_changed.invoke();
    context_.fieldSlices().requestSlice(axis, position, width, height);
    return true;
}

void ApplicationViewModel::setStatus(std::string_view message, int level) {
    {
        std::scoped_lock lock{mutex_};
        level = std::clamp(level, 0, 2);
        if (main_state_.status_message == message && main_state_.status_level == level) {
            return;
        }
        main_state_.status_message = message;
        main_state_.status_level = level;
    }
    main_state_changed.invoke();
}

double ApplicationViewModel::axisExtent() const noexcept {
    return field_state_.axis_index == 2 ? geometry_.length * 0.5 : geometry_.radius;
}

}  // namespace tpc_slint::viewmodels
