#pragma once

#include <span>
#include <string>
#include <vector>

namespace tpc_slint::models {
struct Sensor;
}

namespace tpc_slint::services {
struct RenderedFieldSlice;
}

namespace tpc_slint::viewmodels {

struct FieldTick {
    double position{};  // Normalized image coordinate, including half-pixel edges.
    std::string label;
    bool zero{};
    bool major{true};
};

// Keep colour-scale labels in double precision, even for small changes on a
// large background field. A uniform field has one value at the midpoint.
[[nodiscard]] std::vector<FieldTick> fieldColorTicks(double minimum, double maximum);

struct FieldSensorMarker {
    double u{};
    double v{};
    std::string label;
    bool in_slice{};
};

struct FieldViewport {
    double width{};
    double height{};
    double map_x{};
    double map_y{};
    double map_width{};
    double map_height{};
    bool operator==(const FieldViewport&) const = default;
};

struct FieldGrid {
    std::vector<FieldTick> horizontal;
    std::vector<FieldTick> vertical;
    double major_step{};
};

// Generate one physical spacing for both axes, from the actual screen scale.
// The same ticks drive the plot grid and axes, including the space beyond the TPC.
[[nodiscard]] FieldGrid fieldGrid(const services::RenderedFieldSlice& slice, const FieldViewport& view);

// All configured Hall sensors are projected onto the displayed plane. A star
// identifies a sensor actually in that slice; coincident projections share a label.
[[nodiscard]] std::vector<FieldSensorMarker> fieldSensorMarkers(
    const services::RenderedFieldSlice& slice, std::span<const models::Sensor> sensors
);

}  // namespace tpc_slint::viewmodels
