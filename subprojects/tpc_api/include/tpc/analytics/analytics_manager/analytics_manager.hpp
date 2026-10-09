#pragma once

#include <cmath>

#include <array>
#include <expected>
#include <functional>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <stop_token>
#include <stdexcept>
#include <string>
#include <utility>

#include "tpc/analytics/eigen_module.hpp"
#include "tpc/analytics/models/three_dimension_models.hpp"
#include "tpc/analytics/models/field_quality.hpp"
#include "tpc/analytics/output/vtk_field_exporter.hpp"
#include "tpc/analytics/svd/basis.hpp"
#include "tpc/analytics/svd/svd_solver.hpp"
#include "tpc/core/definitions/analytics_definitions.hpp"
#include "tpc/utilities/header_function.hpp"
namespace tpc::analytics {

class AnalyticsManager final {
    using TargetFunction = utilities::header_function<double(std::size_t, double, double, double)>;
    static constexpr std::size_t kDimension = core::definitions::DIMENSION;

public:
    // Reference callback accepts Cartesian coordinates in the same length unit
    // as the geometry and returns Cartesian field components in gauss.
    using ReferenceField = std::function<std::expected<models::FieldComponents, std::string>(std::array<double, 3>)>;

    void set_reference_field(ReferenceField reference) {
        std::unique_lock coefficients_lock{coefficients_mutex_};
        std::unique_lock field_lock{field_data_mutex_};
        reference_field_ = std::move(reference);
        stored_coefficients_.resize(0);
        field_data_valid_ = false;
    }

    [[nodiscard]] models::FieldQuality field_quality() const {
        std::scoped_lock lock{quality_mutex_};
        return quality_;
    }

    class FieldReadView final {
        friend class AnalyticsManager;

        FieldReadView(std::shared_lock<std::shared_mutex>&& lock, const models::FieldCollection& field_data)
            : lock_(std::move(lock)), field_data_(&field_data) {}

    public:
        FieldReadView(FieldReadView&&) noexcept = default;
        FieldReadView& operator=(FieldReadView&&) noexcept = default;

        FieldReadView(const FieldReadView&) = delete;
        FieldReadView& operator=(const FieldReadView&) = delete;

        [[nodiscard]] std::span<const double> get_coordinates() const noexcept {
            return field_data_->get_coordinates();
        }

        [[nodiscard]] std::span<const double> get_field() const noexcept {
            return field_data_->get_field();
        }

    private:
        std::shared_lock<std::shared_mutex> lock_;
        const models::FieldCollection* field_data_;
    };

    static std::expected<AnalyticsManager, std::string> create(BasisCollection&& basis) {
        if (basis.empty())
            return std::unexpected("Basis functions collection is empty");

        if (basis.get_modes() == 0)
            return std::unexpected("Basis modes count must be greater than zero");

        return AnalyticsManager{std::move(basis)};
    }

private:
    AnalyticsManager(BasisCollection&& basis) : basis_collection_(std::move(basis)) {}

public:
    ~AnalyticsManager() = default;

    AnalyticsManager(const AnalyticsManager&) = delete;
    AnalyticsManager& operator=(const AnalyticsManager&) = delete;

    AnalyticsManager(AnalyticsManager&& other) noexcept
        : basis_collection_(std::move(other.basis_collection_)),
          field_data_(std::move(other.field_data_)),
          field_data_valid_(other.field_data_valid_),
          stored_coefficients_(std::move(other.stored_coefficients_)),
          coordinate_scale_(other.coordinate_scale_),
          reference_field_(std::move(other.reference_field_)),
          limits_(other.limits_), quality_(other.quality_) {}

    AnalyticsManager& operator=(AnalyticsManager&& other) noexcept {
        if (this == &other)
            return *this;

        basis_collection_ = std::move(other.basis_collection_);
        field_data_ = std::move(other.field_data_);
        field_data_valid_ = other.field_data_valid_;
        stored_coefficients_ = std::move(other.stored_coefficients_);
        coordinate_scale_ = other.coordinate_scale_;
        reference_field_ = std::move(other.reference_field_);
        limits_ = other.limits_;
        quality_ = other.quality_;
        return *this;
    }

public:
    std::expected<void, std::string> calculate_svd_coefficients(
        std::span<const models::Measurement> measurements, double threshold = 1e-10,
        models::ReconstructionLimits limits = {}
    ) {
        std::unique_lock coefficients_lock{coefficients_mutex_};
        std::unique_lock field_lock{field_data_mutex_};
        // A failed new fit must never leave a previous map available as current.
        stored_coefficients_.resize(0);
        field_data_valid_ = false;
        if (measurements.empty())
            return std::unexpected("Measurements collection is empty!");
        if (basis_collection_.empty())
            return std::unexpected("Basis is incorrect!");
        if (!std::isfinite(limits.maximum_residual_gauss) || limits.maximum_residual_gauss <= 0.0
            || !std::isfinite(limits.maximum_radial_ratio) || limits.maximum_radial_ratio <= 0.0
            || !std::isfinite(limits.minimum_axial_field_gauss) || limits.minimum_axial_field_gauss <= 0.0)
            return std::unexpected("Reconstruction limits must be finite and positive");

        std::vector<models::Measurement> normalized(measurements.begin(), measurements.end());
        double scale = 0.0;
        for (auto& measurement : normalized) {
            if (measurement.field_components.coordinate_type != models::CoordinateType::Cylindric)
                return std::unexpected("Sensor field components must be expressed as Br, Bphi, Bz in gauss");
            for (const double value : measurement.point_components.components)
                if (!std::isfinite(value)) return std::unexpected("Non-finite sensor position");
            auto transform_result =
                measurement.point_components.transform_coordinates(models::CoordinateType::Cylindric);

            if (!transform_result)
                return std::unexpected(transform_result.error());
            auto& point = measurement.point_components.components;
            scale = std::max({scale, std::abs(point[0]), std::abs(point[2])});
            if (reference_field_) {
                auto reference = reference_field_({point[0] * std::cos(point[1]), point[0] * std::sin(point[1]), point[2]});
                if (!reference) return std::unexpected(reference.error());
                if (reference->coordinate_type != models::CoordinateType::Cartesian)
                    return std::unexpected("Reference map must supply Cartesian components");
                const auto& b = reference->components;
                measurement.field_components.components[0] -= b[0] * std::cos(point[1]) + b[1] * std::sin(point[1]);
                measurement.field_components.components[1] -= -b[0] * std::sin(point[1]) + b[1] * std::cos(point[1]);
                measurement.field_components.components[2] -= b[2];
            }
        }
        const bool degenerate = scale <= 0.0;
        if (!std::isfinite(scale) || (degenerate && !limits.warnings_only))
            return std::unexpected("Degenerate sensor geometry");
        if (degenerate) scale = 1.0;
        // One isotropic length scale preserves the harmonic Maxwell basis.
        for (auto& measurement : normalized) {
            measurement.point_components.components[0] /= scale;
            measurement.point_components.components[2] /= scale;
        }

        models::FieldQuality quality;
        auto result = SVDSolver::solve_svd(
            std::span<const models::Measurement>{normalized},
            basis_collection_.get_basis(),
            basis_collection_.get_modes(),
            threshold, &quality, limits.warnings_only
        );

        if (!result)
            return std::unexpected(result.error());

        quality.fit_within_limit = quality.maximum_residual_gauss <= limits.maximum_residual_gauss;
        quality.geometry_degenerate = degenerate;
        if (!quality.fit_within_limit && !limits.warnings_only)
            return std::unexpected(std::format("Fit rejected: max residual {:.6g} G exceeds {:.6g} G (RMS {:.6g} G)",
                quality.maximum_residual_gauss, limits.maximum_residual_gauss, quality.rms_residual_gauss));

        stored_coefficients_ = std::move(*result);
        coordinate_scale_ = scale;
        limits_ = limits;
        quality.reference_corrected = static_cast<bool>(reference_field_);
        {
            std::scoped_lock quality_lock{quality_mutex_};
            quality_ = quality;
        }

        return {};
    }

    std::expected<models::FieldComponents, std::string> evaluate_for_point(
        std::array<double, kDimension> coordinates
    ) const {
        std::shared_lock lock{coefficients_mutex_};
        const auto modes = basis_collection_.get_modes();

        if (stored_coefficients_.size() != static_cast<Eigen::Index>(modes))
            return std::unexpected("SVD coefficients are not calculated");

        return evaluate_unlocked(coordinates);
    }

    [[nodiscard]] std::expected<models::FieldSlice, std::string> evaluate_field_slice(
        models::SliceDirection direction,
        double coordinate,
        std::array<std::size_t, 2> grid,
        double radius,
        double z_length,
        std::stop_token stop_token = {}
    ) const {
        if (grid[0] < 2 || grid[1] < 2 || grid[0] > 1024 || grid[1] > 1024)
            return std::unexpected("Slice grid dimensions must be at least 2");
        if (!std::isfinite(coordinate) || !std::isfinite(radius * radius) || !std::isfinite(z_length)
            || radius <= 0.0 || z_length <= 0.0)
            return std::unexpected("Radius and length must be positive");

        const double half_length = z_length * 0.5;
        if ((direction == models::SliceDirection::Z && std::abs(coordinate) > half_length)
            || (direction != models::SliceDirection::Z && std::abs(coordinate) > radius))
            return std::unexpected("Slice coordinate is outside the TPC geometry");

        std::shared_lock lock{coefficients_mutex_};
        const auto modes = basis_collection_.get_modes();
        if (stored_coefficients_.size() != static_cast<Eigen::Index>(modes))
            return std::unexpected("SVD coefficients are not calculated");

        models::FieldSlice slice{
            .direction = direction,
            .coordinate = coordinate,
            .grid = grid,
            .field = {},
            .valid = {}
        };
        if (direction == models::SliceDirection::Z) {
            slice.horizontal_bounds = {-radius, radius};
            slice.vertical_bounds = {-radius, radius};
        } else {
            const double transverse_limit = std::sqrt(std::max(0.0, radius * radius - coordinate * coordinate));
            slice.horizontal_bounds = {-transverse_limit, transverse_limit};
            slice.vertical_bounds = {-half_length, half_length};
        }

        if (grid[0] > std::numeric_limits<std::size_t>::max() / grid[1])
            return std::unexpected("Slice grid is too large");
        const std::size_t pixel_count = grid[0] * grid[1];
        if (pixel_count > std::numeric_limits<std::size_t>::max() / kDimension)
            return std::unexpected("Slice grid is too large");
        slice.field.resize(pixel_count * kDimension);
        slice.valid.assign(pixel_count, 0);

        const double du = (slice.horizontal_bounds[1] - slice.horizontal_bounds[0])
            / static_cast<double>(grid[0] - 1);
        const double dv = (slice.vertical_bounds[1] - slice.vertical_bounds[0])
            / static_cast<double>(grid[1] - 1);

        for (std::size_t row = 0; row < grid[1]; ++row) {
            if (stop_token.stop_requested())
                return std::unexpected("Slice evaluation cancelled");
            const double v = slice.vertical_bounds[1] - static_cast<double>(row) * dv;
            for (std::size_t column = 0; column < grid[0]; ++column) {
                const double u = slice.horizontal_bounds[0] + static_cast<double>(column) * du;
                double x{};
                double y{};
                double z{};
                switch (direction) {
                    case models::SliceDirection::X: x = coordinate; y = u; z = v; break;
                    case models::SliceDirection::Y: x = u; y = coordinate; z = v; break;
                    case models::SliceDirection::Z: x = u; y = v; z = coordinate; break;
                }
                if (x * x + y * y > radius * radius)
                    continue;

                const double r = std::hypot(x, y);
                const double phi = r <= std::numeric_limits<double>::epsilon() ? 0.0 : std::atan2(y, x);
                auto evaluated = evaluate_unlocked({r, phi, z});
                if (!evaluated) return std::unexpected(evaluated.error());
                const auto& cylindrical = evaluated->components;

                const std::size_t pixel = row * grid[0] + column;
                const std::size_t offset = pixel * kDimension;
                slice.field[offset] = cylindrical[0] * std::cos(phi) - cylindrical[1] * std::sin(phi);
                slice.field[offset + 1] = cylindrical[0] * std::sin(phi) + cylindrical[1] * std::cos(phi);
                slice.field[offset + 2] = cylindrical[2];
                slice.valid[pixel] = 1;
            }
        }
        return slice;
    }

    std::expected<void, std::string> calculate_field(
        std::array<std::size_t, kDimension> grid_dimensions, double radius, double z_length,
        std::stop_token stop_token = {}
    ) {
        auto validation_result = validate(radius, z_length);

        if (!validation_result)
            return std::unexpected(validation_result.error());

        std::shared_lock coefficients_lock{coefficients_mutex_};

        if (stored_coefficients_.size() != static_cast<Eigen::Index>(basis_collection_.get_modes()))
            return std::unexpected("SVD coefficients are not calculated");

        std::unique_lock field_lock{field_data_mutex_};
        field_data_valid_ = false;

        if (!field_data_)
            field_data_.emplace(kDimension, models::CoordinateType::Cylindric, models::CoordinateType::Cylindric);
        else
            field_data_->clear();

        auto grid_result = calculate_grid_kernel(grid_dimensions, radius, z_length, stop_token);

        if (!grid_result)
            return std::unexpected(grid_result.error());

        field_data_->resize_field_to_coordinates();
        auto field_result = calculate_field_kernel(field_data_->get_coordinates(), field_data_->get_mutable_field(), stop_token);

        if (!field_result)
            return std::unexpected(field_result.error());

        field_data_valid_ = true;
        return {};
    }

    [[nodiscard]] std::expected<FieldReadView, std::string> get_field_data() const {
        std::shared_lock lock{field_data_mutex_};

        if (!field_data_valid_ || !field_data_)
            return std::unexpected("Field data is not calculated");

        return FieldReadView{std::move(lock), *field_data_};
    }

    std::expected<void, std::string> export_to_vtk() {
        std::shared_lock lock{field_data_mutex_};

        if (!field_data_valid_ || !field_data_.has_value())
            return std::unexpected("Field data is not calculated, cannot export to VTK");

        if (field_data_->get_field().empty() || field_data_->get_coordinates().empty())
            return std::unexpected("Field data is empty, cannot export to VTK");

        return tpc::analytics::output::VtkFieldExporter::export_3d_field_to_vtk(
            field_data_->get_field(), field_data_->get_coordinates(), "output.vtk"
        );
    }

private:
    std::expected<void, std::string> calculate_grid_kernel(
        std::array<std::size_t, kDimension> grid_dimensions, double radius, double depth, std::stop_token stop_token
    ) {
        const std::size_t x_count = grid_dimensions[0];
        const std::size_t y_count = grid_dimensions[1];
        const std::size_t z_count = grid_dimensions[2];

        if (x_count < 2 || y_count < 2 || z_count < 2)
            return std::unexpected("Grid dimensions are too small");

        constexpr std::size_t max_points = 8'000'000;
        if (x_count > 1024 || y_count > 1024 || z_count > 1024
            || x_count > max_points / y_count / z_count / 5)
            return std::unexpected("Grid exceeds the reconstruction memory budget");

        if (radius <= 0.0 || depth <= 0.0)
            return std::unexpected("Radius and depth must be positive");

        const double step_x = radius / static_cast<double>(x_count - 1);
        const double step_y = radius / static_cast<double>(y_count - 1) * std::sqrt(3.0) * 0.5;

        const double dz = depth / static_cast<double>(z_count - 1);
        const double radius_squared = radius * radius;

        auto for_each_grid_point = [&](auto&& visitor) {
            for (std::size_t iz = 0; iz < z_count; ++iz) {
                if (stop_token.stop_requested()) return;
                const double z = -depth * 0.5 + static_cast<double>(iz) * dz;
                std::size_t row = 0;

                for (double y = -radius; y <= radius + step_y * 0.5; y += step_y, ++row) {
                    const double x_offset = (row % 2 == 0) ? 0.0 : step_x * 0.5;

                    for (double x = -radius + x_offset; x <= radius + step_x * 0.5; x += step_x) {
                        if (x * x + y * y <= radius_squared)
                            visitor(x, y, z);
                    }
                }
            }
        };

        std::size_t point_count = 0;
        for_each_grid_point([&](double, double, double) {
            ++point_count;
        });
        if (stop_token.stop_requested()) return std::unexpected("Reconstruction cancelled");

        if (point_count > std::numeric_limits<std::size_t>::max() / kDimension)
            return std::unexpected("Grid is too large");

        field_data_->reserve(point_count * kDimension);
        for_each_grid_point([&](double x, double y, double z) {
            const double r = std::hypot(x, y);
            const double phi = r < std::numeric_limits<double>::epsilon() ? 0.0 : std::atan2(y, x);
            field_data_->append_coordinate(r, phi, z);
        });
        if (stop_token.stop_requested()) return std::unexpected("Reconstruction cancelled");

        return {};
    }

    std::expected<void, std::string> calculate_field_kernel(
        std::span<const double> coordinates_buffer, std::span<double> field_buffer, std::stop_token stop_token
    ) {
        if (coordinates_buffer.size() % kDimension != 0)
            return std::unexpected("Coordinate buffer has an invalid size");

        const std::size_t point_count = coordinates_buffer.size() / kDimension;

        if (field_buffer.size() != point_count * kDimension)
            return std::unexpected("Field buffer has an invalid size");

        auto quality = field_quality();
        quality.evaluated_points = point_count;
        quality.maximum_radial_ratio = 0;
        quality.maximum_transverse_ratio = 0;
        quality.minimum_abs_axial_field_gauss = std::numeric_limits<double>::infinity();
        quality.ratio_defined = true;

        for (std::size_t id = 0; id < point_count; ++id) {
            if (id % 1024 == 0 && stop_token.stop_requested())
                return std::unexpected("Reconstruction cancelled");
            const std::size_t offset = id * kDimension;

            const std::array<double, kDimension> coordinates{
                coordinates_buffer[offset],
                coordinates_buffer[offset + 1],
                coordinates_buffer[offset + 2],
            };
            auto evaluated = evaluate_unlocked(coordinates);
            if (!evaluated) return std::unexpected(evaluated.error());
            const auto& components = evaluated->components;
            const double axial = std::abs(components[2]);
            quality.minimum_abs_axial_field_gauss = std::min(quality.minimum_abs_axial_field_gauss, axial);
            if (axial < limits_.minimum_axial_field_gauss) {
                quality.ratio_defined = false;
            } else {
                // At r=0 the radial direction is undefined: use the transverse magnitude conservatively.
                const double radial = coordinates[0] <= coordinate_scale_ * 1e-12
                    ? std::hypot(components[0], components[1]) : std::abs(components[0]);
                quality.maximum_radial_ratio = std::max(quality.maximum_radial_ratio, radial / axial);
                quality.maximum_transverse_ratio = std::max(quality.maximum_transverse_ratio,
                    std::hypot(components[0], components[1]) / axial);
            }

            field_buffer[offset] = components[0];
            field_buffer[offset + 1] = components[1];
            field_buffer[offset + 2] = components[2];
        }

        quality.homogeneous = quality.ratio_defined && quality.maximum_radial_ratio <= limits_.maximum_radial_ratio;
        {
            std::scoped_lock lock{quality_mutex_};
            quality_ = quality;
        }

        return {};
    }


public:
    std::expected<void, std::string> export_to_vtk(std::string_view file_path) {
        std::shared_lock lock{field_data_mutex_};
        if (!field_data_valid_ || !field_data_)
            return std::unexpected("No valid reconstructed field to export");
        if (field_data_->get_field().empty() || field_data_->get_coordinates().empty())
            return std::unexpected("Field data is empty");

        auto result = tpc::analytics::output::VtkFieldExporter::export_3d_field_to_vtk(field_data_->get_field(), field_data_->get_coordinates(), file_path);

        if (!result)
            return std::unexpected(result.error());

        return{};
    }

    static std::array<std::size_t, kDimension> create_grid(std::size_t nx, std::size_t ny, std::size_t nz) {
        return {nx, ny, nz};
    }

    static BasisCollection create_default_basis(
        TargetFunction first, TargetFunction second, TargetFunction third, std::size_t modes
    ) {
        auto basis_result = BasisCollection::create(kDimension, modes, models::CoordinateType::Cylindric);

        if (!basis_result)
            throw std::invalid_argument{basis_result.error()};

        auto basis = std::move(*basis_result);

        (void)basis.add_back(std::move(first));
        (void)basis.add_back(std::move(second));
        (void)basis.add_back(std::move(third));

        return basis;
    }

private:
    // Requires coefficients_mutex_. Coordinates are physical r, phi, z.
    std::expected<models::FieldComponents, std::string> evaluate_unlocked(std::array<double, 3> point) const {
        if (!std::ranges::all_of(point, [](double value) { return std::isfinite(value); }))
            return std::unexpected("Non-finite evaluation coordinates");
        std::array<double, 3> components{};
        const auto basis = basis_collection_.get_basis();
        for (std::size_t mode = 0; mode < basis_collection_.get_modes(); ++mode)
            for (std::size_t component = 0; component < 3; ++component)
                components[component] += stored_coefficients_[static_cast<Eigen::Index>(mode)]
                    * basis[component](mode, point[0] / coordinate_scale_, point[1], point[2] / coordinate_scale_);
        if (reference_field_) {
            auto reference = reference_field_({point[0] * std::cos(point[1]), point[0] * std::sin(point[1]), point[2]});
            if (!reference) return std::unexpected(reference.error());
            if (reference->coordinate_type != models::CoordinateType::Cartesian)
                return std::unexpected("Reference map must supply Cartesian components");
            const auto& b = reference->components;
            components[0] += b[0] * std::cos(point[1]) + b[1] * std::sin(point[1]);
            components[1] += -b[0] * std::sin(point[1]) + b[1] * std::cos(point[1]);
            components[2] += b[2];
        }
        if (!std::ranges::all_of(components, [](double value) { return std::isfinite(value); }))
            return std::unexpected("Non-finite reconstructed field");
        return models::FieldComponents{components, models::CoordinateType::Cylindric};
    }

    std::expected<void, std::string> validate(double radius, double z_length) const {
        const auto basis_dimension = basis_collection_.get_arity();

        if (basis_dimension != kDimension)
            return std::unexpected("Basis does not contain enough components");

        if (!std::isfinite(radius * radius) || !std::isfinite(z_length)
            || radius * radius == 0.0 || radius <= 0.0 || z_length <= 0.0)
            return std::unexpected("Radius and depth must be positive");

        return {};
    }

private:
    BasisCollection basis_collection_;

    std::optional<models::FieldCollection> field_data_;
    bool field_data_valid_{false};
    mutable std::shared_mutex field_data_mutex_;

    third_party::eigen::VectorXd stored_coefficients_;
    mutable std::shared_mutex coefficients_mutex_;
    double coordinate_scale_{1.0};
    ReferenceField reference_field_;
    models::ReconstructionLimits limits_;
    models::FieldQuality quality_;
    mutable std::mutex quality_mutex_;
};
}  // namespace tpc::analytics
