#include "services/tpc_service/tpc_service.hpp"

#include <array>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/scoped_subscription.hpp"
#include "tpc/tpc.hpp"

namespace tpc_slint::services {
namespace {

[[nodiscard]] std::vector<tpc::analytics::models::Measurement> createMeasurements(
    std::span<const models::Sensor> sensors
) {
    std::vector<tpc::analytics::models::Measurement> measurements;
    measurements.reserve(sensors.size());

    for (const auto& sensor : sensors) {
        tpc::analytics::models::PointComponents point{
            .components = sensor.position,
            .coordinate_type = tpc::analytics::models::CoordinateType::Cartesian
        };
        tpc::analytics::models::FieldComponents field{
            .components = sensor.values,
            .coordinate_type = tpc::analytics::models::CoordinateType::Cylindric
        };

        measurements.push_back({.point_components = point, .field_components = field});
    }

    return measurements;
}

}  // namespace

struct TpcServiceBackend final {
    std::unique_ptr<tpc::system::TPC> tpc;

    // Subscriptions are declared after TPC so they are released first.
    ScopedSubscription<tpc::system::client::ConnectionState> connection_subscription;
    ScopedSubscription<tpc::system::models::DiscoveryResult> initialization_subscription;
    ScopedSubscription<std::unordered_map<std::string, double>> frame_subscription;
    ScopedSubscription<bool> field_calculation_subscription;
    ScopedSubscription<const models::AppSettings&> settings_subscription;
};

TpcService::TpcService(EventDispatcher& events)
    : events_(events), backend_(std::make_unique<TpcServiceBackend>()) {
    backend_->settings_subscription.subscribe(
        events_.settings_changed,
        [this](const models::AppSettings& settings) { onSettingsChanged(settings); }
    );
}

TpcService::~TpcService() {
    dispose();
}

bool TpcService::connectAsync(std::string endpoint) {
    // The backend binds its endpoint at construction, so applying a changed
    // endpoint requires replacing the previous instance.
    disconnect();
    std::unique_lock backend_lock{backend_mutex_};
    backend_->connection_subscription.reset();
    backend_->initialization_subscription.reset();
    backend_->frame_subscription.reset();
    backend_->field_calculation_subscription.reset();
    backend_->tpc.reset();

    auto result = tpc::system::TPC::create(endpoint);
    if (!result) {
        return false;
    }

    backend_->tpc = std::move(*result);
    subscribeToBackendEvents();
    backend_->tpc->start_async();
    backend_lock.unlock();
    startPolling();
    return true;
}

void TpcService::disconnect() {
    std::shared_lock backend_lock{backend_mutex_};
    if (backend_->tpc) {
        backend_->tpc->stop_async();
    }
}

void TpcService::calculateField() {
    std::shared_lock backend_lock{backend_mutex_};
    if (!backend_->tpc) {
        return;
    }

    std::vector<tpc::analytics::models::Measurement> measurements;
    std::array<std::size_t, 3> grid{};
    double radius = 0.0;
    double length = 0.0;

    {
        std::scoped_lock lock{data_mutex_};
        measurements = createMeasurements(tpc_data_.sensors());
        grid = tpc_data_.grid();
        radius = tpc_data_.radius();
        length = tpc_data_.length();
    }

    backend_->tpc->calculate_field_async(std::move(measurements), grid, radius, length);
}

std::expected<models::NumericFieldSlice, std::string> TpcService::calculateFieldSlice(
    int axis,
    double coordinate,
    std::array<std::size_t, 2> grid,
    std::stop_token stop_token
) {
    std::shared_lock backend_lock{backend_mutex_};
    if (!backend_->tpc) {
        return std::unexpected{"TPC backend is not initialized"};
    }

    double radius = 0.0;
    double length = 0.0;
    {
        std::scoped_lock lock{data_mutex_};
        radius = tpc_data_.radius();
        length = tpc_data_.length();
    }

    const auto direction = axis == 0 ? tpc::analytics::models::SliceDirection::X
        : axis == 1 ? tpc::analytics::models::SliceDirection::Y
                    : tpc::analytics::models::SliceDirection::Z;
    auto result = backend_->tpc->get_field_slice(direction, coordinate, grid, radius, length, stop_token);
    if (!result) {
        return std::unexpected{result.error()};
    }

    return models::NumericFieldSlice{
        .grid = result->grid,
        .horizontal_bounds = result->horizontal_bounds,
        .vertical_bounds = result->vertical_bounds,
        .field = std::move(result->field),
        .valid = std::move(result->valid)
    };
}

bool TpcService::exportFieldToVtk(std::string_view file_path) {
    std::shared_lock backend_lock{backend_mutex_};
    if (!backend_->tpc) {
        return false;
    }

    return backend_->tpc->export_to_vtk(file_path).has_value();
}

void TpcService::dispose() noexcept {
    backend_->connection_subscription.reset();
    backend_->initialization_subscription.reset();
    backend_->frame_subscription.reset();
    backend_->settings_subscription.reset();
    backend_->field_calculation_subscription.reset();

    std::unique_lock backend_lock{backend_mutex_};
    if (backend_->tpc) {
        backend_->tpc->stop_async();
    }
    backend_->tpc.reset();
}

void TpcService::subscribeToBackendEvents() {
    backend_->connection_subscription.subscribe(
        backend_->tpc->connection_state_changed_,
        [this](tpc::system::client::ConnectionState state) {
            const bool connected = state == tpc::system::client::ConnectionState::Connected
                || state == tpc::system::client::ConnectionState::SessionActivated;
            events_.connection_state_changed.invoke(connected);
        }
    );
    backend_->initialization_subscription.subscribe(
        backend_->tpc->initialization_data_received_,
        [this](tpc::system::models::DiscoveryResult result) {
            const auto names = models::SensorName::parseNames(result.nodes | std::views::values);
            if (names) {
                events_.initialization_data_received.invoke(*names);
            }
        }
    );
    backend_->frame_subscription.subscribe(
        backend_->tpc->frame_received_,
        [this](std::unordered_map<std::string, double> frame) { onFrameReceived(std::move(frame)); }
    );
    backend_->field_calculation_subscription.subscribe(
        backend_->tpc->field_was_calculated_,
        [this](bool is_calculated) { onFieldWasCalculated(is_calculated); }
    );
}

void TpcService::startPolling() {
    std::size_t interval = 0;
    {
        std::scoped_lock lock{data_mutex_};
        interval = polling_interval_ms_;
    }

    std::shared_lock backend_lock{backend_mutex_};
    if (backend_->tpc && interval > 0) {
        backend_->tpc->start_polling_async(interval);
    }
}

void TpcService::onSettingsChanged(const models::AppSettings& settings) {
    std::vector<models::Sensor> sensors;
    sensors.reserve(settings.sensors_info.size());

    for (const auto& sensor : settings.sensors_info) {
        sensors.push_back({.name = sensor.name, .position = {sensor.x, sensor.y, sensor.z}});
    }

    {
        std::scoped_lock lock{data_mutex_};
        tpc_data_.setSensors(std::move(sensors));
        tpc_data_.setGeometry(settings.geometry.length, settings.geometry.radius);
        tpc_data_.setGrid(settings.grid);
        polling_interval_ms_ = static_cast<std::size_t>(settings.connection.polling_interval);
    }

    startPolling();
}

void TpcService::onFrameReceived(std::unordered_map<std::string, double> frame) {
    {
        std::scoped_lock lock{data_mutex_};
        tpc_data_.setReceivedFrame(frame);
    }

    events_.frame_received.invoke(frame);
}

void TpcService::onFieldWasCalculated(bool is_calculated) {
    events_.field_was_calculated_.invoke(is_calculated);
}

}  // namespace tpc_slint::services
