#pragma once

#include <cstddef>
#include <cmath>
#include <expected>
#include <limits>
#include <span>
#include <string>

#include "tpc/analytics/eigen_module.hpp"
#include "tpc/analytics/concepts/measurement_concept.hpp"
#include "tpc/analytics/models/three_dimension_models.hpp"
#include "tpc/analytics/models/field_quality.hpp"
#include "tpc/utilities/header_function.hpp"
namespace tpc::analytics {
class SVDSolver {
public:
    template <concepts::MeasurementConcept Measurement>
    static std::expected<third_party::eigen::VectorXd, std::string> solve_svd(
        std::span<const Measurement> measurements,
        std::span<const utilities::header_function<double(std::size_t, double, double, double)>> basis_functions,
        std::size_t modes,
        double threshold,
        models::FieldQuality* quality = nullptr,
        bool warnings_only = false
    ) {
        if (measurements.empty())
            return std::unexpected("Measurement collection is empty");

        if (basis_functions.empty())
            return std::unexpected("Basis functions collection is empty");

        if (modes == 0)
            return std::unexpected("Modes count must be greater than zero");

        if (!std::isfinite(threshold) || threshold < 0.0 || threshold >= 1.0)
            return std::unexpected("SVD threshold must be finite and in [0, 1)");

        const std::size_t field_dimension = measurements.front().get_point_components().size();

        if (field_dimension == 0)
            return std::unexpected("Measurement dimension must be greater than zero");

        if (basis_functions.size() != field_dimension)
            return std::unexpected("Number of basis functions must equal field dimension");

        if (measurements.size() > static_cast<std::size_t>(std::numeric_limits<Eigen::Index>::max()) / field_dimension)
            return std::unexpected("Measurement matrix is too large");

        if (modes > static_cast<std::size_t>(std::numeric_limits<Eigen::Index>::max()))
            return std::unexpected("Modes count is too large");

        const auto rows = static_cast<Eigen::Index>(measurements.size() * field_dimension);
        const auto columns = static_cast<Eigen::Index>(modes);
        if (rows < columns && !warnings_only)
            return std::unexpected("Insufficient measurements for the selected basis");
        third_party::eigen::MatrixXd matrix(rows, columns);
        third_party::eigen::VectorXd values_vector(rows);

        for (std::size_t measurement_index = 0; measurement_index < measurements.size(); ++measurement_index) {
            const auto coordinates = measurements[measurement_index].get_point_components();
            const auto values = measurements[measurement_index].get_field_components();

            if (coordinates.size() != field_dimension || values.size() != field_dimension)
                return std::unexpected("Measurement dimensions are inconsistent");

            for (std::size_t component = 0; component < field_dimension; ++component) {
                const auto row = static_cast<Eigen::Index>(field_dimension * measurement_index + component);
                values_vector(row) = values[component];
                if (!std::isfinite(values[component]))
                    return std::unexpected("Non-finite magnetic field measurement");

                for (std::size_t mode = 0; mode < modes; ++mode) {
                    matrix(row, static_cast<Eigen::Index>(mode)) =
                        basis_functions[component].invoke_from_span(mode, coordinates);
                }
            }
        }

        if (!matrix.allFinite())
            return std::unexpected("Non-finite coordinates or basis values");

        // Column equilibration makes the numerical threshold independent of
        // different polynomial degrees. Undo it after solving.
        auto normalized = matrix;
        third_party::eigen::VectorXd scales(columns);
        for (Eigen::Index column = 0; column < columns; ++column) {
            scales(column) = matrix.col(column).stableNorm();
            if (!std::isfinite(scales(column)))
                return std::unexpected("Non-finite basis scale");
            if (scales(column) <= 0.0 && !warnings_only)
                return std::unexpected("Unobservable basis mode: check sensor positions");
            if (scales(column) <= 0.0) scales(column) = 1.0;
            normalized.col(column) /= scales(column);
        }

        third_party::eigen::JacobiSVD<third_party::eigen::MatrixXd> svd(
            normalized, third_party::eigen::ComputeThinU | third_party::eigen::ComputeThinV
        );
        svd.setThreshold(threshold);

        if (svd.info() != Eigen::Success)
            return std::unexpected("SVD decomposition failed");
        if (svd.rank() != columns && !warnings_only)
            return std::unexpected("Rank-deficient reconstruction: sensor geometry cannot determine all modes");

        third_party::eigen::VectorXd coefficients = svd.solve(values_vector).cwiseQuotient(scales);
        if (!coefficients.allFinite())
            return std::unexpected("Non-finite reconstruction coefficients");
        const third_party::eigen::VectorXd residual = matrix * coefficients - values_vector;
        if (quality) {
            quality->measurement_count = measurements.size();
            quality->rank = static_cast<std::size_t>(svd.rank());
            quality->condition_number = svd.rank() == columns
                ? svd.singularValues()(0) / svd.singularValues()(columns - 1)
                : std::numeric_limits<double>::infinity();
            quality->maximum_residual_gauss = residual.cwiseAbs().maxCoeff();
            quality->rms_residual_gauss = residual.stableNorm() / std::sqrt(static_cast<double>(rows));
        }
        return coefficients;
    }
};
}  // namespace tpc::analytics
