module;
#include <string>
export module tpc_qt.models.ui.application_settings_model;
export namespace tpc_qt::models {
    struct Sensor {
        std::string name{};
        float x{0.f};
        float y{0.f};
        float z{0.f};
    };

    struct SensorsSettings {
        std::vector<Sensor> sensors{};
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
        SensorsSettings sensors{};
    };
}
