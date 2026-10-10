#pragma once

#include <cstddef>
#include <limits>

namespace tpc::analytics::models {

// Limits are independent: numerical rank, fit consistency and magnetic homogeneity.
struct ReconstructionLimits {
    double svd_threshold{1e-10};
    double maximum_residual_gauss{0.2};
    double maximum_radial_ratio{5.2e-4};
    double minimum_axial_field_gauss{1.0};
    bool warnings_only{false};
};

struct FieldQuality {
    std::size_t measurement_count{};
    std::size_t rank{};
    double condition_number{};
    double maximum_residual_gauss{};
    double rms_residual_gauss{};
    double maximum_radial_ratio{};
    double maximum_transverse_ratio{};
    double minimum_abs_axial_field_gauss{std::numeric_limits<double>::infinity()};
    std::size_t evaluated_points{};
    bool ratio_defined{};
    bool homogeneous{};
    bool reference_corrected{};
    bool fit_within_limit{};
    bool geometry_degenerate{};
};

} // namespace tpc::analytics::models
