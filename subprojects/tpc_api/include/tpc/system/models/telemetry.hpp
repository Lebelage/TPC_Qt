#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

namespace tpc::system::models {
struct ChannelSample {
    double value{};
    std::chrono::steady_clock::time_point received_at{};
    std::chrono::system_clock::time_point source_time{};
    bool good{};
    bool has_source_time{};
    bool in_gauss{};
};
using TelemetryFrame = std::unordered_map<std::string, ChannelSample>;
}
