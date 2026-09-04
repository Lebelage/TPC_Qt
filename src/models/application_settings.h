#pragma once
#include <string>

namespace tpc_qt::models {
    struct ConnectionParameters {
        std::string endpoint{};
        int polling_interval{};
    };

    struct TpcGeometricParams {
        double length;
        double radius;
    };

    struct AppSettings {
        TpcGeometricParams geometric{0, 0};
        ConnectionParameters connection{"", 0};
    };
}
