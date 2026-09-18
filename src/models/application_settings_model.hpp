#pragma once
#include <string>
#include <vector>

#include "models/tpc_data_model.hpp"
#include "nlohmann/json.hpp"
namespace tpc_qt::models {
struct SensorInfo {
    SensorName name{};

    std::string previewable_name{};
    float x{0.f};
    float y{0.f};
    float z{0.f};

    void set_name(SensorName name) {
        this->name = name;
        this->previewable_name = name.to_string();
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
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SensorInfo, previewable_name, x, y, z)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConnectionParameters, endpoint, polling_interval)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TpcGeometryParams, length, radius)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AppSettings, geometry, connection, sensors_info)
}  // namespace tpc_qt::models
