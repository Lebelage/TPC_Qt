#include "tpc/system/tpc.hpp"

#include <chrono>
#include <cmath>
#include <expected>
#include <format>
#include <iostream>
#include <numbers>
#include <optional>
#include <unordered_map>
#include <utility>

#include "tpc/analytics/analytics_manager/analytics_manager.hpp"
#include "tpc/analytics/models/basis_models.hpp"
#include "tpc/analytics/models/three_dimension_models.hpp"
#include "tpc/core/definitions/client_definitions.hpp"
#include "tpc/system/models/data.hpp"
namespace tpc::system {

struct AnalyticsImpl {
    analytics::AnalyticsManager analytics_manager_;
};

#pragma region Factory / Constructor

std::expected<std::unique_ptr<TPC>, std::string> TPC::create(std::string_view endpoint) {
    try {
        return std::unique_ptr<TPC>{new TPC(std::string{endpoint})};
    } catch (const std::exception& error) {
        return std::unexpected{
            std::format("[{}]: Failed to create TPC device: {}", core::definitions::CLIENT_ERROR, error.what())
        };
    } catch (...) {
        return std::unexpected{"Failed to create TPC: unknown error"};
    }
}

TPC::TPC(std::string endpoint) {
    auto result = client::Client::create(std::move(endpoint));

    if (!result) {
        throw std::runtime_error{result.error()};
    }

    client_ = std::move(*result);
    initialize_start_handlers();

    auto basis = analytics::AnalyticsManager::create_default_basis(
        analytics::models::DefaultBasisR,
        analytics::models::DefaultBasisPhi,
        analytics::models::DefaultBasisZ,
        analytics::models::DEFAULT_MODES_COUNT_
    );

    auto analytics_create_result = tpc::analytics::AnalyticsManager::create(std::move(basis));

    if (!analytics_create_result)
        throw std::runtime_error{analytics_create_result.error()};

    impl_ = std::make_unique<AnalyticsImpl>(AnalyticsImpl{.analytics_manager_ = std::move(*analytics_create_result)});
}

TPC::~TPC() = default;

#pragma endregion

#pragma region Public Methods

auto TPC::start_async() -> void {
    auto result = client_->connect_async();

    if (!result)
        error_occurred_.invoke(result.error());
    else if (!*result)
        warning_occurred_.invoke("TPC client is already running");
}

auto TPC::stop_async() -> void {
    {
        std::lock_guard lock{polling_worker_mutex_};
        polling_worker_.request_stop();
    }
    polling_worker_cv_.notify_all();

    client_->stop();
}

auto TPC::is_running() const -> bool {
    return client_->is_running();
}

auto TPC::get_frame_request() -> std::optional<std::unordered_map<std::string, double>> {
    const auto samples = get_telemetry_request();
    std::unordered_map<std::string, double> frame;
    for (const auto& [name, sample] : samples)
        if (sample.good) frame.emplace(name, sample.value);
    if (frame.empty()) return std::nullopt;
    return frame;
}

auto TPC::get_telemetry_request() -> models::TelemetryFrame {
    auto frame = client_->get_telemetry();
    for (auto& [sensor_name, sample] : frame) {
        if (input_in_gauss_) {
            sample.in_gauss = true;
            continue;
        }
        const auto calibration = models::HallCalibrationCollection::find(sensor_name);
        if (calibration) {
            sample.value = millivolts_to_gauss(sample.value, *calibration);
            sample.in_gauss = true;
            sample.good = sample.good && std::isfinite(sample.value);
        }
    }
    return frame;
}

std::expected<void, std::string> TPC::set_reference_field(analytics::AnalyticsManager::ReferenceField reference) {
    std::lock_guard lock{field_worker_mutex_};
    if (field_calculation_in_progress_)
        return std::unexpected("Cannot change reference map during reconstruction");
    impl_->analytics_manager_.set_reference_field(std::move(reference));
    return {};
}

auto TPC::create_test_frame() -> std::unordered_map<std::string, double> {
    constexpr std::size_t sensors_per_base = 6;
    constexpr double radial_field_at_end_gauss = 0.5;
    constexpr double axial_field_at_sensors_gauss = 5000.0;
    constexpr double angular_variation_gauss = 0.02;
    constexpr double radius = models::TpcGeometry::kRadius;
    constexpr double half_length = models::TpcGeometry::kLength * 0.5;

    // Axisymmetric, divergence-free second-order approximation:
    //   Br = -a*r*z
    //   Bz = B0 + a*(z^2 - r^2/2)
    // Br therefore changes sign at the cylinder centre, while Bz is even in Z
    // and has a shallow maximum on the axis near the centre of the solenoid.
    constexpr double axial_curvature = -radial_field_at_end_gauss / (radius * half_length);
    constexpr double axial_offset = axial_field_at_sensors_gauss
        - axial_curvature * (half_length * half_length - 0.5 * radius * radius);

    // This mode adds a small, basis-compatible angular imperfection:
    //   Br += c*z*cos(phi), Bphi += -c*z*sin(phi), Bz += c*r*cos(phi).
    constexpr double angular_coefficient = angular_variation_gauss / half_length;

    std::unordered_map<std::string, double> frame;
    frame.reserve(2 * sensors_per_base * 3);

    for (const char base : {'E', 'W'}) {
        const double z = base == 'W' ? half_length : -half_length;

        for (std::size_t index = 0; index < sensors_per_base; ++index) {
            const double phase = 2.0 * std::numbers::pi * static_cast<double>(index)
                / static_cast<double>(sensors_per_base);
            const std::string sensor_name = std::string(1, base) + std::to_string(index + 1);

            const double radial_field = -axial_curvature * radius * z
                + angular_coefficient * z * std::cos(phase);
            // A constant Bphi bias is not a source-free regular harmonic field.
            const double azimuthal_field = -angular_coefficient * z * std::sin(phase);
            const double axial_field = axial_offset
                + axial_curvature * (z * z - 0.5 * radius * radius)
                + angular_coefficient * radius * std::cos(phase);

            frame.emplace(sensor_name + 'R', radial_field);
            frame.emplace(sensor_name + 'F', azimuthal_field);
            frame.emplace(sensor_name + 'Z', axial_field);
        }
    }

    return frame;
}

auto TPC::start_polling_async(size_t polling_interval_ms) -> void {
    if (polling_interval_ms == 0) {
        warning_occurred_.invoke("Polling interval must be greater than zero");
        return;
    }

    std::jthread stopped_worker;
    {
        std::lock_guard lock{polling_worker_mutex_};

        polling_interval_ms_ = polling_interval_ms;
        ++polling_interval_revision_;

        if (polling_worker_.joinable() && polling_worker_.get_stop_token().stop_requested())
            stopped_worker = std::move(polling_worker_);

        if (!polling_worker_.joinable()) {
            polling_worker_ = std::jthread([this](std::stop_token stop_token) {
                polling_worker_loop(stop_token);
            });
        }
    }

    polling_worker_cv_.notify_all();
}

auto TPC::calculate_field_async(std::vector<analytics::models::Measurement> measurements, std::array<size_t, tpc::core::definitions::DIMENSION> grid, double radius, double length,
    analytics::models::ReconstructionLimits limits) -> std::expected<void, std::string> {
    if (measurements.empty())
        return std::unexpected("No sensor measurements available");

    bool calculation_already_in_progress = false;

    {
        std::lock_guard lock{field_worker_mutex_};

        if (field_calculation_in_progress_) {
            calculation_already_in_progress = true;
        } else {
            if (!field_worker_.joinable()) {
                field_worker_ = std::jthread([this](std::stop_token stop_token) {
                    field_worker_loop(stop_token);
                });
            }

            models::CalculationData calculation_data{
                .grid = std::move(grid),
                .measurements = std::move(measurements),
                .radius = radius,
                .length = length,
                .limits = limits
            };

            pending_calculation_data_ = std::move(calculation_data);
            field_calculation_in_progress_ = true;
        }
    }

    if (calculation_already_in_progress) {
        return std::unexpected("Field calculation is already in progress");
    }

    field_worker_cv_.notify_one();
    return {};
}

auto TPC::get_field_slice(
    analytics::models::SliceDirection direction,
    double coordinate,
    std::array<std::size_t, 2> grid,
    double radius,
    double length,
    std::stop_token stop_token
) const -> std::expected<analytics::models::FieldSlice, std::string> {
    return impl_->analytics_manager_.evaluate_field_slice(direction, coordinate, grid, radius, length, stop_token);
}

auto TPC::export_to_vtk(std::string_view file_path) -> std::expected<void, std::string> {

    if (file_path.empty())
        return std::unexpected("File path is empty");

    auto result = impl_->analytics_manager_.export_to_vtk(file_path);

    if (!result) {
        auto error = std::format("Exporting vtk file failed: {}", result.error());
        return std::unexpected(error);
    }
    return {};
}

#pragma endregion

#pragma region Private Initialization

double TPC::millivolts_to_gauss(double voltage_mv, const models::HallCalibration& calibration) noexcept {
    return calibration.k * (voltage_mv - calibration.v0_mv);
}

auto TPC::initialize_start_handlers() -> void {
    (void)client_->error_occurred_.subscribe([this](const std::string& err) {
        on_client_error(err);
    });

    (void)client_->info_occurred_.subscribe([this](const std::string& info) {
        on_client_info(info);
    });

    (void)client_->connection_state_changed_.subscribe([this](client::ConnectionState state) {
        on_client_connection_state_changed(state);
    });

    (void)client_->initialization_data_received_.subscribe([this](const models::DiscoveryResult& discovery_result) {
        on_client_initialization_data_received(discovery_result);
    });
}

auto TPC::field_worker_loop(std::stop_token stop_token) -> void {
    while (!stop_token.stop_requested()) {
        models::CalculationData calculation_data;

        {
            std::unique_lock lock{field_worker_mutex_};
            const bool has_work = field_worker_cv_.wait(lock, stop_token, [this] {
                return pending_calculation_data_.has_value();
            });

            if (!has_work)
                return;

            calculation_data = std::move(*pending_calculation_data_);
            pending_calculation_data_.reset();
        }

        bool calculation_succeeded = false;
        std::string error;

        try {
            auto coefficients_result = impl_->analytics_manager_.calculate_svd_coefficients(calculation_data.measurements,
                calculation_data.limits.svd_threshold, calculation_data.limits);

            if (!coefficients_result) {
                error = std::move(coefficients_result.error());
            } else {
                auto field_result = impl_->analytics_manager_.calculate_field(calculation_data.grid, calculation_data.radius, calculation_data.length, stop_token);

                if (!field_result)
                    error = std::move(field_result.error());
                else {
                    calculation_succeeded = true;
                }

            }
        } catch (const std::exception& exception) {
            error = exception.what();
        } catch (...) {
            error = "Unknown error during field calculation";
        }

        {
            std::lock_guard lock{field_worker_mutex_};
            field_calculation_in_progress_ = false;
        }

        if (!calculation_succeeded) {
            try {
                error_occurred_.invoke(std::format("Field calculation failed: {}", error));
            } catch (...) {
                // A user callback .
            }
        }

        try {
            if (calculation_succeeded)
                field_quality_changed_.invoke(impl_->analytics_manager_.field_quality());
            field_was_calculated_.invoke(calculation_succeeded);
        } catch (...) {

            // A user callback.
        }
    }
}

auto TPC::polling_worker_loop(std::stop_token stop_token) -> void {
    while (!stop_token.stop_requested()) {
        try {
            auto telemetry = get_telemetry_request();
            std::unordered_map<std::string, double> frame;
            for (const auto& [name, sample] : telemetry)
                if (sample.good) frame.emplace(name, sample.value);
            telemetry_received_.invoke(std::move(telemetry));
            frame_received_.invoke(std::move(frame));
        } catch (const std::exception& exception) {
            try {
                error_occurred_.invoke(std::format("Frame polling failed: {}", exception.what()));
            } catch (...) {
            }
        } catch (...) {
            try {
                error_occurred_.invoke("Frame polling failed: unknown error");
            } catch (...) {
            }
        }

        std::unique_lock lock{polling_worker_mutex_};
        const auto interval = std::chrono::milliseconds{polling_interval_ms_};
        const auto revision = polling_interval_revision_;

        polling_worker_cv_.wait_for(lock, stop_token, interval, [this, revision] {
            return polling_interval_revision_ != revision;
        });
    }
}

#pragma endregion

#pragma region Event Handlers

auto TPC::on_client_error(const std::string& err) -> void {
    error_occurred_.invoke(err);
}

auto TPC::on_client_info(const std::string& info) -> void {
    info_occurred_.invoke(info);
}

auto TPC::on_client_connection_state_changed(client::ConnectionState state) -> void {
    connection_state_changed_.invoke(state);
}

auto TPC::on_client_initialization_data_received(const models::DiscoveryResult& discovery_result) -> void {
    initialization_data_received_.invoke(discovery_result);
}

#pragma endregion

}  // namespace tpc::system
