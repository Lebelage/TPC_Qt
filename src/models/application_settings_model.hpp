#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "models/tpc_data_model.hpp"
#include "nlohmann/json.hpp"
namespace tpc_slint::models {
struct ChannelCalibration {
    double k{1};
    double v0_mv{};
};

/** Persisted Cartesian coordinates in millimetres for one physical sensor. */
struct SensorInfo {
    SensorName name{};

    std::string previewable_name{};
    float x{0.f};
    float y{0.f};
    float z{0.f};
    bool enabled{true};
    bool position_configured{true};
    std::unordered_map<std::string, ChannelCalibration> calibration;
    // Discovery belongs to the current connection and is never persisted.
    bool available{};

    [[nodiscard]] bool selectable() const noexcept {
        return available && position_configured;
    }
    [[nodiscard]] bool active() const noexcept {
        return selectable() && enabled;
    }

    void setName(SensorName name) {
        this->name = name;
        previewable_name = name.toString();
    }
};

struct ConnectionParameters {
    std::string endpoint{};
    int polling_interval{};
};

struct TpcGeometryParams {
    // Millimetres.
    double length{};
    double radius{};
};

struct AppSettings {
    // Monotonic publication order, never read from or written to JSON.
    std::uint64_t runtime_revision{};
    std::string coordinate_unit{"mm"};
    TpcGeometryParams geometry{0, 0};
    ConnectionParameters connection{"", 0};
    std::vector<SensorInfo> sensors_info{};
    std::array<std::size_t, 3> grid{};
    struct AnalysisParameters {
        double maximum_residual_gauss{0.2};
        double maximum_radial_ratio{5.2e-4};
        double minimum_axial_field_gauss{1.0};
        int maximum_sample_age_ms{3000};
        int maximum_frame_skew_ms{1000};
        bool input_in_gauss{false};
        std::string reference_map_path;
    } analysis;
};

// SensorName is reconstructed from previewable_name (and later confirmed by
// discovery), so its implementation representation is intentionally not persisted.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChannelCalibration, k, v0_mv)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    SensorInfo, previewable_name, x, y, z, enabled, position_configured, calibration
)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConnectionParameters, endpoint, polling_interval)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TpcGeometryParams, length, radius)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    AppSettings::AnalysisParameters,
    maximum_residual_gauss,
    maximum_radial_ratio,
    minimum_axial_field_gauss,
    maximum_sample_age_ms,
    maximum_frame_skew_ms,
    input_in_gauss,
    reference_map_path
)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    AppSettings, coordinate_unit, geometry, connection, sensors_info, grid, analysis
)
}  // namespace tpc_slint::models
