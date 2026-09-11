module;
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
export module tpc_qt.models.ui.application_settings_model;
export namespace tpc_qt::models {
    struct SensorInfo {
        std::string group_name{};
        float x{0.f};
        float y{0.f};
        float z{0.f};
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

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        SensorInfo, group_name, x, y, z)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        ConnectionParameters, endpoint, polling_interval)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        TpcGeometryParams, length, radius)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        AppSettings, geometry, connection, sensors_info)
}
