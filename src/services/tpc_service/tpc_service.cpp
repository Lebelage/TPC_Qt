#include "services/tpc_service/tpc_service.hpp"

#include <array>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/scoped_subscription.hpp"
#include "tpc/tpc.hpp"
#include "models/measurement_quality.hpp"
#include "models/reference_field_map.hpp"

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
    ScopedSubscription<tpc::system::models::TelemetryFrame> telemetry_subscription;
    ScopedSubscription<tpc::analytics::models::FieldQuality> quality_subscription;
    ScopedSubscription<std::string> error_subscription;
    ScopedSubscription<std::string> warning_subscription;
    models::MeasurementFrame samples;
    models::ScientificStatus scientific_status;
    bool connected{};
    bool calculation_calibrated{};
    std::string calculation_warning;
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
    backend_->telemetry_subscription.reset();
    backend_->quality_subscription.reset();
    backend_->error_subscription.reset();
    backend_->warning_subscription.reset();
    {
        std::scoped_lock data_lock{data_mutex_};
        backend_->samples.clear();
        backend_->connected = false;
    }
    backend_->tpc.reset();

    auto result = tpc::system::TPC::create(endpoint);
    if (!result) {
        return false;
    }

    backend_->tpc = std::move(*result);
    {
        std::scoped_lock data_lock{data_mutex_};
        backend_->tpc->set_input_in_gauss(settings_snapshot_.analysis.input_in_gauss);
    }
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

std::expected<void, std::string> TpcService::calculateField() {
    std::shared_lock backend_lock{backend_mutex_};
    if (!backend_->tpc) {
        return std::unexpected("Connect to TPC before calculating the field");
    }

    std::vector<tpc::analytics::models::Measurement> measurements;
    std::array<std::size_t, 3> grid{};
    double radius = 0.0;
    double length = 0.0;
    tpc::analytics::models::ReconstructionLimits limits;
    std::string reference_path;
    models::ScientificStatus status;

    {
        std::scoped_lock lock{data_mutex_};
        field_snapshot_valid_ = false;
        calculation_revision_ = settings_revision_;
        const auto prepared = models::preparePreliminaryMeasurements(settings_snapshot_, backend_->samples);
        if (!prepared) return std::unexpected(prepared.error());
        measurements = createMeasurements(prepared->sensors);
        backend_->calculation_warning = prepared->warning;
        if (!backend_->connected) backend_->calculation_warning += " | Offline snapshot";
        backend_->scientific_status.field_message = backend_->calculation_warning.empty()
            ? "Calculating field snapshot" : "WARNING: " + backend_->calculation_warning;
        backend_->scientific_status.homogeneous = false;
        status = backend_->scientific_status;
        backend_->calculation_calibrated = models::measurementsInGauss(settings_snapshot_, backend_->samples);
        grid = tpc_data_.grid();
        radius = tpc_data_.radius();
        length = tpc_data_.length();
        limits.maximum_residual_gauss = settings_snapshot_.analysis.maximum_residual_gauss;
        limits.maximum_radial_ratio = settings_snapshot_.analysis.maximum_radial_ratio;
        limits.minimum_axial_field_gauss = settings_snapshot_.analysis.minimum_axial_field_gauss;
        limits.warnings_only = true;
        reference_path = settings_snapshot_.analysis.reference_map_path;
    }
    events_.scientific_status_changed.invoke(status);

    if (!reference_path.empty()) {
        auto reference = models::ReferenceFieldMap::load(reference_path);
        if (!reference) return std::unexpected(reference.error());
        auto configured = backend_->tpc->set_reference_field([map = std::move(*reference)](std::array<double, 3> point)
            -> std::expected<tpc::analytics::models::FieldComponents, std::string> {
            auto value = map->evaluate(point);
            if (!value) return std::unexpected(value.error());
            return tpc::analytics::models::FieldComponents{*value, tpc::analytics::models::CoordinateType::Cartesian};
        });
        if (!configured) return configured;
    } else {
        auto configured = backend_->tpc->set_reference_field({});
        if (!configured) return configured;
    }
    return backend_->tpc->calculate_field_async(std::move(measurements), grid, radius, length, limits);
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
        if (!field_snapshot_valid_) return std::unexpected("No field snapshot for the current settings");
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
    {
        std::scoped_lock lock{data_mutex_};
        if (!field_snapshot_valid_) return false;
    }
    if (!backend_->tpc) {
        return false;
    }

    const auto exported = backend_->tpc->export_to_vtk(file_path);
    events_.diagnostic.invoke(exported ? LogLevel::Info : LogLevel::Error, "export",
        exported ? "VTK saved: " + std::string{file_path} : "VTK export failed: " + exported.error());
    return exported.has_value();
}

void TpcService::dispose() noexcept {
    backend_->connection_subscription.reset();
    backend_->initialization_subscription.reset();
    backend_->frame_subscription.reset();
    backend_->settings_subscription.reset();
    backend_->field_calculation_subscription.reset();
    backend_->telemetry_subscription.reset();
    backend_->quality_subscription.reset();
    backend_->error_subscription.reset();
    backend_->warning_subscription.reset();

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
            const bool connected = state == tpc::system::client::ConnectionState::SessionActivated;
            models::ScientificStatus status;
            {
                std::scoped_lock lock{data_mutex_};
                backend_->connected = connected;
                if (!connected) backend_->samples.clear();
                backend_->scientific_status.data_ready = false;
                backend_->scientific_status.data_message = connected ? "Waiting for fresh 36-channel frame" : "Offline: no current measurements";
                status = backend_->scientific_status;
            }
            events_.connection_state_changed.invoke(connected);
            events_.scientific_status_changed.invoke(status);
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
    backend_->error_subscription.subscribe(backend_->tpc->error_occurred_, [this](std::string error) {
        events_.error_occurred.invoke(std::move(error));
    });
    backend_->warning_subscription.subscribe(backend_->tpc->warning_occurred_, [this](std::string warning) {
        events_.diagnostic.invoke(LogLevel::Warning, "opcua", warning);
    });
    backend_->telemetry_subscription.subscribe(backend_->tpc->telemetry_received_, [this](auto frame) {
        models::ScientificStatus status;
        bool changed = false;
        {
            std::scoped_lock lock{data_mutex_};
            backend_->samples.clear();
            for (const auto& [name, sample] : frame)
                backend_->samples.emplace(name, models::MeasurementSample{sample.value, sample.received_at,
                    sample.source_time, sample.good, sample.has_source_time, sample.in_gauss});
            const auto valid = models::validatedMeasurements(settings_snapshot_, backend_->samples);
            const std::string message = !valid ? valid.error()
                : models::measurementsInGauss(settings_snapshot_, backend_->samples)
                    ? "36/36 channels: fresh, coherent, in gauss"
                    : "36/36 channels: preliminary calculation allowed; calibration incomplete (G / mV)";
            changed = backend_->scientific_status.data_message != message
                || backend_->scientific_status.data_ready != (static_cast<bool>(valid) && backend_->connected);
            backend_->scientific_status.data_ready = static_cast<bool>(valid) && backend_->connected;
            backend_->scientific_status.data_message = message;
            status = backend_->scientific_status;
        }
        if (changed) events_.scientific_status_changed.invoke(status);
    });
    backend_->quality_subscription.subscribe(backend_->tpc->field_quality_changed_, [this](auto quality) {
        models::ScientificStatus status;
        {
            std::scoped_lock lock{data_mutex_};
            backend_->scientific_status.homogeneous = quality.homogeneous && backend_->calculation_calibrated
                && backend_->calculation_warning.empty() && quality.fit_within_limit && quality.rank == 10;
            backend_->scientific_status.field_message = !backend_->calculation_calibrated
                ? std::format("PRELIMINARY: calibration incomplete (G / mV) | rank {} | max residual {:.4g}, RMS {:.4g} | physical accuracy and Br/Bz not verified",
                    quality.rank, quality.maximum_residual_gauss, quality.rms_residual_gauss)
                : std::format(
                "Snapshot: {} | rank {} | max residual {:.4g} G, RMS {:.4g} G | sampled |Br/Bz|={:.5g} | {}",
                quality.reference_corrected ? "Reference + correction" : "Standalone harmonic approximation",
                quality.rank, quality.maximum_residual_gauss, quality.rms_residual_gauss, quality.maximum_radial_ratio,
                !quality.ratio_defined ? "UNDEFINED: Bz too small" : quality.homogeneous ? "Within configured limit" : "HOMOGENEITY LIMIT EXCEEDED");
            if (!backend_->calculation_warning.empty())
                backend_->scientific_status.field_message += " | WARNING: " + backend_->calculation_warning;
            if (!quality.fit_within_limit)
                backend_->scientific_status.field_message += " | WARNING: fit residual exceeds configured limit";
            if (quality.rank != 10 || quality.geometry_degenerate)
                backend_->scientific_status.field_message += " | WARNING: underdetermined geometry; minimum-norm preview, not a verified field";
            status = backend_->scientific_status;
        }
        events_.scientific_status_changed.invoke(status);
    });
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
        ++settings_revision_;
        field_snapshot_valid_ = false;
        tpc_data_.setSensors(std::move(sensors));
        settings_snapshot_ = settings;
        tpc_data_.setGeometry(settings.geometry.length, settings.geometry.radius);
        tpc_data_.setGrid(settings.grid);
        polling_interval_ms_ = static_cast<std::size_t>(settings.connection.polling_interval);
    }

    {
        std::shared_lock backend_lock{backend_mutex_};
        if (backend_->tpc) backend_->tpc->set_input_in_gauss(settings.analysis.input_in_gauss);
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
    models::ScientificStatus status;
    {
        std::scoped_lock lock{data_mutex_};
        is_calculated = is_calculated && calculation_revision_ == settings_revision_;
        field_snapshot_valid_ = is_calculated;
        if (!is_calculated) {
            backend_->scientific_status.homogeneous = false;
            backend_->scientific_status.field_message = "No verified reconstruction for the current settings";
        }
        status = backend_->scientific_status;
    }
    events_.scientific_status_changed.invoke(status);
    events_.field_was_calculated_.invoke(is_calculated);
}

}  // namespace tpc_slint::services
