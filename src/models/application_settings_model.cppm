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

    struct SensorsInfoCollection {
        std::vector<SensorInfo> sensors{};
    };

    struct ConnectionParameters {
        std::string endpoint{};
        int polling_interval{};
    };

    struct TpcGeometricParams {
        double length{0};
        double radius{0};
    };

    struct AppSettings {
        TpcGeometricParams geometric{0, 0};
        ConnectionParameters connection{"", 0};
        SensorsInfoCollection sensors_info{};
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        SensorInfo, group_name, x, y, z)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        SensorsInfoCollection, sensors)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        ConnectionParameters, endpoint, polling_interval)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        TpcGeometricParams, length, radius)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
        AppSettings, geometric, connection, sensors_info)
}
