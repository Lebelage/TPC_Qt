#pragma once

#include <chrono>
#include <cmath>
#include <expected>
#include <format>
#include <unordered_set>

#include "models/application_settings_model.hpp"

namespace tpc_slint::models {

struct MeasurementSample {
    double value{};
    std::chrono::steady_clock::time_point received_at;
    std::chrono::system_clock::time_point source_time;
    bool good{};
    bool has_source_time{};
    bool in_gauss{};
};
using MeasurementFrame = std::unordered_map<std::string, MeasurementSample>;

inline bool measurementsInGauss(const AppSettings& settings, const MeasurementFrame& frame) {
    for (const auto& sensor : settings.sensors_info)
        for (const char component : {'R', 'F', 'Z'}) {
            const auto sample = frame.find(sensor.previewable_name + component);
            if (sample == frame.end() || !sample->second.in_gauss) return false;
        }
    return !settings.sensors_info.empty();
}

/** Validate exactly the 36 configured channels; never fill missing samples with zero. */
inline std::expected<std::vector<Sensor>, std::string> validatedMeasurements(
    const AppSettings& settings, const MeasurementFrame& frame,
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now(),
    std::chrono::system_clock::time_point source_now = std::chrono::system_clock::now()
) {
    if (settings.sensors_info.size() != 12)
        return std::unexpected("TPC requires exactly 12 configured sensors (6 E + 6 W)");
    if (!std::isfinite(settings.geometry.radius) || !std::isfinite(settings.geometry.length)
        || settings.geometry.radius <= 0 || settings.geometry.length <= 0)
        return std::unexpected("Invalid TPC geometry");
    std::unordered_set<std::string> identities;
    std::vector<Sensor> sensors;
    std::size_t east = 0;
    auto oldest_source = std::chrono::system_clock::time_point::max();
    auto newest_source = std::chrono::system_clock::time_point::min();
    const auto maximum_age = std::chrono::milliseconds{settings.analysis.maximum_sample_age_ms};
    for (const auto& info : settings.sensors_info) {
        const auto name = SensorName::parse(info.previewable_name);
        if (!name || name->toString() != info.previewable_name || name->number < 1 || name->number > 6
            || !identities.insert(info.previewable_name).second)
            return std::unexpected("Invalid or duplicate sensor identity");
        east += name->id == SensorNameKey::E;
        Sensor sensor{.name = *name, .position = {info.x, info.y, info.z}};
        if (!std::isfinite(info.x) || !std::isfinite(info.y) || !std::isfinite(info.z)
            || (name->id == SensorNameKey::E ? info.z <= 0 : info.z >= 0))
            return std::unexpected("Sensor positions must be finite and on the respective TPC ends");
        for (const auto& previous : sensors)
            if (previous.position == sensor.position)
                return std::unexpected("Two sensors occupy the same position");
        static constexpr std::array<char, 3> components{'R', 'F', 'Z'};
        for (std::size_t component = 0; component < 3; ++component) {
            const auto channel = info.previewable_name + components[component];
            const auto found = frame.find(channel);
            if (found == frame.end()) return std::unexpected("Missing channel: " + channel);
            const auto& sample = found->second;
            if (!sample.good || !std::isfinite(sample.value))
                return std::unexpected("Bad OPC UA sample: " + channel);
            if (!sample.has_source_time)
                return std::unexpected("Source timestamp missing: " + channel);
            if (sample.received_at > now || now - sample.received_at > maximum_age
                || source_now - sample.source_time > maximum_age
                || sample.source_time - source_now > std::chrono::seconds{1})
                return std::unexpected("Stale sample or unsynchronized source clock: " + channel);
            oldest_source = std::min(oldest_source, sample.source_time);
            newest_source = std::max(newest_source, sample.source_time);
            sensor.values[component] = sample.value;
        }
        sensors.push_back(sensor);
    }
    if (east != 6) return std::unexpected("TPC requires 6 sensors on each end");
    if (newest_source - oldest_source > std::chrono::milliseconds{settings.analysis.maximum_frame_skew_ms})
        return std::unexpected("The 36 channels do not form a time-coherent frame");
    return sensors;
}

struct PreparedMeasurements {
    std::vector<Sensor> sensors;
    std::string warning;
};

// Interactive mode retains finite received values despite quality warnings.
// Missing components use an explicitly reported zero placeholder, never silently.
inline std::expected<PreparedMeasurements, std::string> preparePreliminaryMeasurements(
    const AppSettings& settings, const MeasurementFrame& frame
) {
    if (settings.sensors_info.empty()) return std::unexpected("No sensor positions configured");
    if (!std::isfinite(settings.geometry.radius) || !std::isfinite(settings.geometry.length)
        || settings.geometry.radius <= 0 || settings.geometry.length <= 0)
        return std::unexpected("Cannot build a grid with invalid geometry");
    PreparedMeasurements result;
    if (auto checked = validatedMeasurements(settings, frame); !checked) result.warning = checked.error();
    if (!measurementsInGauss(settings, frame)) result.warning += " | Calibration incomplete (G / mV)";
    std::size_t received = 0, missing = 0;
    for (const auto& info : settings.sensors_info) {
        auto name = SensorName::parse(info.previewable_name);
        if (!name) return std::unexpected(name.error());
        Sensor sensor{.name = *name, .position = {info.x, info.y, info.z}};
        for (double coordinate : sensor.position)
            if (!std::isfinite(coordinate)) return std::unexpected("Non-finite sensor coordinates");
        const std::array<char, 3> components{'R', 'F', 'Z'};
        for (std::size_t component = 0; component < components.size(); ++component) {
            const auto sample = frame.find(info.previewable_name + components[component]);
            if (sample == frame.end()) { ++missing; continue; }
            if (!std::isfinite(sample->second.value)) return std::unexpected("Non-finite channel value");
            sensor.values[component] = sample->second.value;
            ++received;
        }
        result.sensors.push_back(sensor);
    }
    if (!received) return std::unexpected("No received channel values to calculate from");
    if (missing) result.warning += std::format(" | {} missing components filled with zero for preview", missing);
    if (result.warning.starts_with(" | ")) result.warning.erase(0, 3);
    return result;
}

struct ScientificStatus {
    bool data_ready{};
    std::string data_message{"Waiting for 36 fresh channels"};
    std::string field_message{"No verified reconstruction"};
    bool homogeneous{};
};

} // namespace tpc_slint::models
