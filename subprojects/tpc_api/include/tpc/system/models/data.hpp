#pragma once

#include <array>
#include <cstddef>
#include <open62541pp/client.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

#include "tpc/core/definitions/analytics_definitions.hpp"
#include "tpc/analytics/models/three_dimension_models.hpp"
#include "tpc/analytics/models/field_quality.hpp"

namespace tpc::system::models {

struct NodeIdHash {
    std::size_t operator()(const opcua::NodeId& node_id) const noexcept {
        return static_cast<std::size_t>(node_id.hash());
    }
};

struct NodeIdEqual {
    bool operator()(const opcua::NodeId& lhs, const opcua::NodeId& rhs) const noexcept {
        return lhs == rhs;
    }
};

struct DiscoveryResult {
    std::unordered_map<opcua::NodeId, std::string, NodeIdHash, NodeIdEqual> nodes;
};

struct ReceivedItem {
    std::string name{};
    double value{};
};

struct HallCalibration {
    double k;      // Гс/мВ
    double v0_mv;  // мВ
};

struct NamedHallCalibration {
    std::string_view name;
    HallCalibration calibration;
};

struct HallCalibrationCollection {
    // Hall coefs (2).txt joined with Hall number to name (2).txt by SensorNum.
    // B [G] = k [G/mV] * (V [mV] - V0 [mV]).
    static inline constexpr std::array kCalibrations{
        NamedHallCalibration{"E1R", {.k = 0.569788, .v0_mv = -25.37}}, // 212
        NamedHallCalibration{"E1F", {.k = 0.747579, .v0_mv = 10.46}},  // 469
        NamedHallCalibration{"E1Z", {.k = 0.751044, .v0_mv = -3.22}},  // 481
        NamedHallCalibration{"E2R", {.k = 0.584679, .v0_mv = 39.96}},  // 182
        NamedHallCalibration{"E2F", {.k = 0.751875, .v0_mv = -21.96}}, // 428
        NamedHallCalibration{"E2Z", {.k = 0.765028, .v0_mv = 15.54}},  // 441
        NamedHallCalibration{"E3R", {.k = 0.588650, .v0_mv = 12.94}},  // 134
        NamedHallCalibration{"E3F", {.k = 0.768763, .v0_mv = 11.29}},  // 466
        NamedHallCalibration{"E3Z", {.k = 0.758018, .v0_mv = -2.42}},  // 426
        NamedHallCalibration{"E4R", {.k = 0.588823, .v0_mv = 57.86}},  // 174
        NamedHallCalibration{"E4F", {.k = 0.783179, .v0_mv = 24.69}},  // 592
        NamedHallCalibration{"E4Z", {.k = 0.588173, .v0_mv = 8.00}},   // 188
        NamedHallCalibration{"E5R", {.k = 0.737055, .v0_mv = 29.63}},  // 425
        NamedHallCalibration{"E5F", {.k = 0.741515, .v0_mv = 26.63}},  // 419
        NamedHallCalibration{"E5Z", {.k = 0.753643, .v0_mv = -2.89}},  // 449
        NamedHallCalibration{"E6R", {.k = 0.581638, .v0_mv = 50.80}},  // 168
        NamedHallCalibration{"E6F", {.k = 0.758680, .v0_mv = -4.27}},  // 451
        NamedHallCalibration{"E6Z", {.k = 0.737996, .v0_mv = 4.33}},   // 461
        NamedHallCalibration{"E7R", {.k = 0.584204, .v0_mv = 2.23}},   // 113
        NamedHallCalibration{"E7F", {.k = 0.767363, .v0_mv = 6.21}},   // 407
        NamedHallCalibration{"E7Z", {.k = 0.754259, .v0_mv = 9.47}},   // 418
        NamedHallCalibration{"E8R", {.k = 0.591976, .v0_mv = -29.53}}, // 119
        NamedHallCalibration{"E8F", {.k = 0.783120, .v0_mv = 18.29}},  // 440
        NamedHallCalibration{"E8Z", {.k = 0.756729, .v0_mv = 11.58}},  // 422
        NamedHallCalibration{"W1R", {.k = 0.571794, .v0_mv = -39.06}}, // 215
        NamedHallCalibration{"W1F", {.k = 0.578475, .v0_mv = -8.52}},  // 270
        NamedHallCalibration{"W1Z", {.k = 0.576308, .v0_mv = 33.82}},  // 203
        NamedHallCalibration{"W2R", {.k = 0.580946, .v0_mv = 14.09}},  // 158
        NamedHallCalibration{"W2F", {.k = 0.752525, .v0_mv = 5.53}},   // 450
        NamedHallCalibration{"W2Z", {.k = 0.743973, .v0_mv = 27.22}},  // 416
        NamedHallCalibration{"W3R", {.k = 0.594150, .v0_mv = -2.20}},  // 165
        NamedHallCalibration{"W3F", {.k = 0.744115, .v0_mv = -28.17}}, // 432
        NamedHallCalibration{"W3Z", {.k = 0.764318, .v0_mv = 12.06}},  // 424
        NamedHallCalibration{"W4R", {.k = 0.596320, .v0_mv = -0.23}},  // 150
        NamedHallCalibration{"W4F", {.k = 0.770527, .v0_mv = 32.09}},  // 420
        NamedHallCalibration{"W4Z", {.k = 0.784851, .v0_mv = 4.55}},   // 489
        NamedHallCalibration{"W5R", {.k = 0.749723, .v0_mv = 10.26}},  // 463
        NamedHallCalibration{"W5F", {.k = 0.752230, .v0_mv = 21.58}},  // 447
        NamedHallCalibration{"W5Z", {.k = 0.719524, .v0_mv = -2.34}},  // 406
        NamedHallCalibration{"W6R", {.k = 0.583520, .v0_mv = 31.64}},  // 199
        NamedHallCalibration{"W6F", {.k = 0.744100, .v0_mv = 23.09}},  // 431
        NamedHallCalibration{"W6Z", {.k = 0.758395, .v0_mv = 25.48}},  // 459
        NamedHallCalibration{"W7R", {.k = 0.579138, .v0_mv = 15.08}},  // 213
        NamedHallCalibration{"W7F", {.k = 0.768619, .v0_mv = 13.65}},  // 444
        NamedHallCalibration{"W7Z", {.k = 0.770980, .v0_mv = 13.08}},  // 468
        NamedHallCalibration{"W8R", {.k = 0.595289, .v0_mv = 60.07}},  // 40
        NamedHallCalibration{"W8F", {.k = 0.602996, .v0_mv = -3.95}},  // 266
        NamedHallCalibration{"W8Z", {.k = 0.598474, .v0_mv = 29.74}}   // 35
    };

    [[nodiscard]]
    static constexpr const HallCalibration* find(std::string_view name) noexcept {
        for (const auto& entry : kCalibrations) {
            if (entry.name == name) {
                return &entry.calibration;
            }
        }

        return nullptr;
    }
};

struct TpcGeometry {
    // Millimetres; initial volume bounded by the sensor ring and end planes.
    static inline constexpr double kRadius = 1408.569;
    static inline constexpr double kLength = 4058;
};

struct CalculationData {
    std::array<size_t, tpc::core::definitions::DIMENSION> grid;
    std::vector<analytics::models::Measurement> measurements;

    double radius;
    double length;
    analytics::models::ReconstructionLimits limits;
};
}  // namespace tpc::system::models
