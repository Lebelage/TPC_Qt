#pragma once

#include <memory>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

#include "models/field_slice_model.hpp"
#include "models/tpc_data_model.hpp"
#include "models/measurement_quality.hpp"
#include "services/scoped_subscription.hpp"
#include "tpc/utilities/event_handler.hpp"

namespace tpc_slint::application {
class ApplicationContext;
}

namespace tpc_slint::models {
struct AppSettings;
}

namespace tpc_slint::services {
struct RenderedFieldSlice;
}

namespace tpc_slint::viewmodels {

struct SensorRowViewState {
    std::string name;
    std::string radial;
    std::string azimuthal;
    std::string longitudinal;
    std::string coordinate_x;
    std::string coordinate_y;
    std::string coordinate_z;

    bool operator==(const SensorRowViewState&) const = default;
};

struct MainWindowViewState {
    // Immutable snapshots keep status/settings notifications independent of table size.
    std::shared_ptr<const std::vector<SensorRowViewState>> sensor_rows{
        std::make_shared<const std::vector<SensorRowViewState>>()};
    bool connected{};
    bool field_calculated{};
    bool calculating{};
    models::ScientificStatus scientific_status;
    std::string tpc_length{"4058"};
    std::string tpc_radius{"1408.569"};
    std::string endpoint{"opc.tcp://127.0.0.1:1234"};
    std::string polling_interval{"1000"};
    std::string grid_nx{"32"};
    std::string grid_ny{"32"};
    std::string grid_nz{"32"};
    std::string status_message{"Ready"};
    int status_level{};
};

struct FieldWindowViewState {
    bool available{};
    bool loading{};
    int axis_index{2};
    float slice_position{};
    std::string position_input{"0"};
    float minimum_position{};
    float maximum_position{};
    float minimum_value{};
    float maximum_value{};
    std::string position_label{"—"};
    std::shared_ptr<const services::RenderedFieldSlice> rendered_slice;
};

struct SettingsInput {
    std::string length;
    std::string radius;
    std::string endpoint;
    std::string polling_interval;
    std::string grid_nx;
    std::string grid_ny;
    std::string grid_nz;
};

/**
 * Toolkit-independent application state and commands.
 *
 * The Slint view observes state-changed events and forwards user
 * commands here. Services and domain models never depend on generated UI types.
 */
class ApplicationViewModel final {
public:
    explicit ApplicationViewModel(application::ApplicationContext& context);
    ~ApplicationViewModel();

    ApplicationViewModel(const ApplicationViewModel&) = delete;
    ApplicationViewModel& operator=(const ApplicationViewModel&) = delete;

    [[nodiscard]] MainWindowViewState mainState() const;
    [[nodiscard]] FieldWindowViewState fieldState() const;

    void showStartupError(std::string_view message);
    void connect();
    void disconnect();
    void calculateField();
    void reloadSettings();
    void applySettings(SettingsInput input);
    void exportField(std::string_view path);
    void selectAxis(int axis);
    void selectPosition(float position);
    void selectPosition(std::string_view position);
    void setViewport(float width, float height);
    [[nodiscard]] std::string inspectField(float x, float y, float width, float height,
        const std::shared_ptr<const services::RenderedFieldSlice>& displayed_slice) const;

    tpc::utilities::event_handler<> main_state_changed;
    tpc::utilities::event_handler<> settings_state_changed;
    tpc::utilities::event_handler<> field_state_changed;

private:
    void bindServiceEvents();
    void acceptSettings(const models::AppSettings& settings);
    void acceptFrame(const models::TpcDataModel::ReceivedFrame& frame);
    void acceptGeometry(models::FieldGeometry geometry);
    void acceptSlice(std::shared_ptr<const services::RenderedFieldSlice> slice);
    // These state helpers require mutex_ to be held by the caller.
    void rebuildSensorRows();
    void resetSlicePosition();
    [[nodiscard]] double axisExtent() const noexcept;

    void selectPositionValue(double position);
    [[nodiscard]] bool requestSlice();
    void setStatus(std::string_view message, int level = 0);

    application::ApplicationContext& context_;
    mutable std::mutex mutex_;
    MainWindowViewState main_state_;
    FieldWindowViewState field_state_;
    std::vector<models::Sensor> sensors_;
    std::unordered_map<std::uint32_t, std::size_t> sensor_indices_;
    models::FieldGeometry geometry_;
    double position_{};
    int viewport_width_{640};
    int viewport_height_{520};

    services::ScopedSubscription<const models::AppSettings&> settings_subscription_;
    services::ScopedSubscription<const models::TpcDataModel::ReceivedFrame&> frame_subscription_;
    services::ScopedSubscription<bool> connection_subscription_;
    services::ScopedSubscription<bool> calculation_subscription_;
    services::ScopedSubscription<const models::ScientificStatus&> scientific_subscription_;
    services::ScopedSubscription<std::string> error_subscription_;
    std::string calculation_error_;
    services::ScopedSubscription<models::FieldGeometry> geometry_subscription_;
    services::ScopedSubscription<std::shared_ptr<const services::RenderedFieldSlice>> slice_subscription_;
    services::ScopedSubscription<std::string> slice_failure_subscription_;
};

}  // namespace tpc_slint::viewmodels
