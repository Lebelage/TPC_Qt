#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "models/tpc_data_model.hpp"
#include "nlohmann/json.hpp"
namespace tpc_qt::models {
/** Persisted coordinates for one physical sensor. */
struct SensorInfo {
    SensorName name{};

    std::string previewable_name{};
    float x{0.f};
    float y{0.f};
    float z{0.f};

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
    double length{};
    double radius{};
};

struct AppSettings {
    TpcGeometryParams geometry{0, 0};
    ConnectionParameters connection{"", 0};
    std::vector<SensorInfo> sensors_info{};
    std::array<std::size_t, 3> grid{};
};

// SensorName is reconstructed from previewable_name (and later confirmed by
// discovery), so its implementation representation is intentionally not persisted.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SensorInfo, previewable_name, x, y, z)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConnectionParameters, endpoint, polling_interval)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TpcGeometryParams, length, radius)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AppSettings, geometry, connection, sensors_info, grid)
}  // namespace tpc_qt::models
