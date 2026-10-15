#include <cmath>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "application/application_context.hpp"
#include "models/measurement_quality.hpp"
#include "tpc/system/models/data.hpp"
#include "viewmodels/application_view_model.hpp"

namespace m = tpc_slint::models;
namespace vm = tpc_slint::viewmodels;
namespace fs = std::filesystem;

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
struct TemporaryDirectory {
    fs::path path = fs::temp_directory_path() /
                    ("tpc-profiles-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryDirectory() {
        fs::create_directories(path);
    }
    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};
void write(const fs::path& path, std::string_view text) {
    std::ofstream file(path);
    file << text;
    require(bool(file), "Fixture was written");
}
std::string read(const fs::path& path) {
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), {}};
}
const m::SensorInfo& sensor(const m::AppSettings& settings, std::string_view name) {
    const auto found = std::ranges::find(settings.sensors_info, name, &m::SensorInfo::previewable_name);
    require(found != settings.sensors_info.end(), "Sensor retained in merged configuration");
    return *found;
}
const vm::SensorRowViewState& row(const vm::MainWindowViewState& state, std::string_view name) {
    const auto found = std::ranges::find(*state.sensor_rows, name, &vm::SensorRowViewState::name);
    require(found != state.sensor_rows->end(), "Sensor appears in Workspace");
    return *found;
}
std::vector<std::string> channelsFor(std::initializer_list<std::string> names) {
    std::vector<std::string> channels;
    for (const auto& name : names)
        for (char component : {'R', 'F', 'Z'})
            channels.push_back(name + component);
    return channels;
}
void profilesAndSelection() {
    TemporaryDirectory directory;
    tpc_slint::application::ApplicationContext context(directory.path / "default.json");
    vm::ApplicationViewModel view(context);
    require(context.initialize().has_value(), "Default configuration loads");
    auto defaults = context.settings().currentSettings();
    require(
        defaults.sensors_info.size() == 12 && view.mainState().enabled_sensor_count == 0,
        "Before discovery all configured sensors are OFF and unavailable"
    );
    auto profile = defaults;
    profile.connection.endpoint = "opc.tcp://example.test:4840";
    profile.sensors_info = {sensor(defaults, "E1"), sensor(defaults, "W1"), sensor(defaults, "E5")};
    profile.sensors_info[0].x = 321;
    profile.sensors_info[0].calibration["R"] = {2, 3};
    profile.sensors_info[1].enabled = false;
    const auto file = directory.path / "custom.json";
    const auto original = nlohmann::json(profile).dump(4);
    write(file, original);
    view.loadSettings(file.string());
    require(
        context.settings().settingsPath() == file && view.mainState().endpoint == profile.connection.endpoint,
        "Selecting a profile updates active file and connection parameters"
    );
    require(read(file) == original, "Opening a user profile never rewrites it");
    auto channels = channelsFor({"E1", "W1", "W2", "E99"});
    channels.insert(channels.end(), {"E2R", "E2Z", "not-a-sensor", "W01R", "W0Z"});
    auto discovery = m::discoverSensors(channels);
    require(discovery.size() == 5, "Discovery groups channel names and ignores malformed identities");
    context.events().connection_state_changed.invoke(true);
    context.events().initialization_data_received.invoke(discovery);
    auto current = context.settings().currentSettings();
    require(current.sensors_info.size() == 6, "Merge is a union, retaining file-only and server-only sensors");
    require(
        sensor(current, "E1").active() && sensor(current, "E1").x == 321 &&
            sensor(current, "E1").calibration.at("R").k == 2,
        "Merge preserves file coordinates and calibration"
    );
    require(
        !sensor(current, "W1").active() && sensor(current, "W1").selectable(), "Saved manual OFF remains selectable"
    );
    require(
        !sensor(current, "E5").available && !row(view.mainState(), "E5").selectable,
        "File-only sensors remain visible but cannot be enabled"
    );
    require(
        sensor(current, "W2").active() && sensor(current, "W2").z == 2029,
        "Server-only known sensors receive standard geometry and start ON"
    );
    require(
        sensor(current, "E99").available && !sensor(current, "E99").selectable() &&
            row(view.mainState(), "E99").status == "Coordinates required",
        "Unknown server-only sensors are added without inventing a position"
    );
    require(!sensor(current, "E2").available, "A missing F channel makes a discovered sensor unavailable");
    require(view.mainState().enabled_sensor_count == 2, "Workspace counts only selected usable sensors");
    require(read(file) == original, "Discovery leaves the source profile untouched");
    const auto now = std::chrono::steady_clock::now();
    const auto source = std::chrono::system_clock::now();
    m::MeasurementFrame frame;
    for (const auto& channel : channelsFor({"E1", "W1", "W2", "E99"}))
        frame.emplace(channel, m::MeasurementSample{10, now, source, true, true, true});
    frame["E5R"].value = std::numeric_limits<double>::quiet_NaN();
    auto prepared = m::preparePreliminaryMeasurements(current, frame);
    require(
        prepared && prepared->sensors.size() == 2 && prepared->sensors[0].name.toString() == "E1" &&
            prepared->sensors[1].name.toString() == "W2",
        "Reconstruction excludes disabled and unavailable sensors entirely"
    );
    require(
        m::validatedMeasurements(current, frame, now, source).has_value(),
        "Arbitrary selected sensor counts are accepted"
    );
    const auto obsolete = current;
    require(context.settings().setSensorEnabled("E1", false).has_value(), "An available sensor can be disabled");
    context.events().settings_changed.invoke(obsolete);
    require(!row(view.mainState(), "E1").enabled, "An older settings publication cannot undo a newer manual choice");
    context.events().initialization_data_received.invoke(discovery);
    current = context.settings().currentSettings();
    require(
        !sensor(current, "E1").enabled && !row(view.mainState(), "E1").enabled,
        "Repeated discovery does not re-enable a manually disabled sensor"
    );
    prepared = m::preparePreliminaryMeasurements(current, frame);
    require(
        prepared && prepared->sensors.size() == 1 && prepared->sensors[0].name.toString() == "W2",
        "The next field reconstruction uses only the remaining selection"
    );
    require(
        !context.settings().setSensorEnabled("E5", true) && !context.settings().setSensorEnabled("E99", true),
        "Unavailable and unconfigured sensors cannot be enabled through the service API"
    );
    require(context.settings().setSensorEnabled("W2", false).has_value(), "Last active sensor can be disabled");
    require(
        !m::preparePreliminaryMeasurements(context.settings().currentSettings(), frame),
        "An empty selection fails instead of reconstructing zero measurements"
    );
    context.settings().applySettings();
    const auto persisted = nlohmann::json::parse(read(file));
    require(
        !persisted["sensors_info"][0].contains("available") && !persisted["sensors_info"][0]["enabled"].get<bool>(),
        "Apply saves user selection but never persists server availability"
    );
    context.events().connection_state_changed.invoke(false);
    require(
        view.mainState().enabled_sensor_count == 0 && !row(view.mainState(), "W1").selectable,
        "Disconnect removes availability without removing rows"
    );
    context.events().initialization_data_received.invoke(discovery);
    require(
        view.mainState().enabled_sensor_count == 0 && !row(view.mainState(), "W1").selectable,
        "Late discovery from a stopped session cannot restore availability"
    );
    context.events().connection_state_changed.invoke(true);
    context.events().initialization_data_received.invoke(discovery);
    require(!sensor(context.settings().currentSettings(), "E1").enabled, "Manual preference survives reconnect");
    context.events().initialization_data_received.invoke({});
    require(
        context.settings().currentSettings().sensors_info.size() == 6 && view.mainState().enabled_sensor_count == 0,
        "Empty discovery disables all sensors and preserves configured identities"
    );
    const auto bad = directory.path / "bad.json";
    write(bad, "{broken");
    view.loadSettings(bad.string());
    require(
        context.settings().settingsPath() == file && read(bad) == "{broken",
        "Invalid profile preserves both active settings and the invalid source file"
    );
    require(
        !context.settings().loadSettings(directory.path / "missing.json") &&
            !fs::exists(directory.path / "missing.json"),
        "An explicitly selected missing profile is not replaced by defaults"
    );
    auto invalid = nlohmann::json(profile);
    invalid["sensors_info"][0]["calibration"]["R"]["k"] = 0;
    write(bad, invalid.dump());
    require(
        !context.settings().loadSettings(bad) && context.settings().settingsPath() == file,
        "Invalid calibration is rejected transactionally"
    );
    invalid = nlohmann::json(profile);
    invalid["sensors_info"].push_back(invalid["sensors_info"][0]);
    write(bad, invalid.dump());
    require(!context.settings().loadSettings(bad), "Duplicate configured identities are rejected");
    const auto second = directory.path / "second.json";
    profile.sensors_info.resize(1);
    profile.sensors_info[0].calibration["R"] = {7, -4};
    write(second, nlohmann::json(profile).dump());
    view.loadSettings(second.string());
    require(
        context.settings().settingsPath() == second && view.mainState().sensor_rows->size() == 1 &&
            !view.mainState().connected && !row(view.mainState(), "E1").selectable,
        "Switching profiles replaces configuration and requires fresh discovery"
    );
    context.settings().applySettings();
    require(read(file) == persisted.dump(4), "Saving the next profile leaves the previous profile intact");
    auto minimal = nlohmann::json(profile);
    minimal["sensors_info"] = nlohmann::json::array({{{"previewable_name", "W1"}}, {{"previewable_name", "E99"}}});
    minimal["analysis"]["reference_map_path"] = "maps/reference.json";
    write(second, minimal.dump());
    require(context.settings().loadSettings(second).has_value(), "Sensor names without coordinates can be loaded");
    current = context.settings().currentSettings();
    require(
        sensor(current, "W1").position_configured && sensor(current, "W1").z == 2029 &&
            !sensor(current, "E99").position_configured,
        "Omitted positions use measured defaults only when known"
    );
    require(
        current.analysis.reference_map_path == (directory.path / "maps/reference.json").string(),
        "Relative reference paths resolve from the selected profile directory"
    );
    minimal["sensors_info"][0]["x"] = 1;
    write(bad, minimal.dump());
    require(!context.settings().loadSettings(bad), "Partial XYZ coordinates are rejected");
    write(second, "");
    require(
        !context.settings().loadSettings() && read(second).empty() && context.settings().settingsPath() == second,
        "Reload of an empty user profile preserves the file and current runtime configuration"
    );
}
void calibrationOverrides() {
    using namespace tpc::system::models;
    std::unordered_map<std::string, HallCalibration> custom{
        {"E1R", {2, 3}}
    };
    const auto* overridden = HallCalibrationCollection::resolve("E1R", custom);
    require(overridden && overridden->apply(13) == 20, "File calibration uses B = k * (U - v0_mv)");
    const auto* fallback = HallCalibrationCollection::resolve("W1R", custom);
    require(
        fallback && fallback == HallCalibrationCollection::find("W1R") &&
            std::abs(fallback->apply(10) - .571794 * (10 + 39.06)) < 1e-12,
        "Unspecified channels retain built-in coefficients"
    );
    require(
        !HallCalibrationCollection::resolve("E99R", custom), "Unknown channels are not assigned fabricated calibration"
    );
}
}  // namespace
int main() {
    try {
        profilesAndSelection();
        calibrationOverrides();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Sensor profiles, discovery, selection and calibration checks passed\n";
}
