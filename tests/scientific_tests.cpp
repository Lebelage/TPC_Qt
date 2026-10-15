#include "tpc/analytics/analytics_manager/analytics_manager.hpp"
#include "tpc/analytics/models/basis_models.hpp"
#include "tpc/tpc.hpp"
#include "models/measurement_quality.hpp"
#include "models/reference_field_map.hpp"
#include "viewmodels/field_map_overlay.hpp"
#include "services/settings_holder/settings_holder.hpp"
#include "services/file_worker/file_worker.hpp"
#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/field_slice/rendered_field_slice.hpp"
#include <iostream>
#include <numbers>
#include <stdexcept>

namespace a = tpc::analytics;
namespace m = tpc_slint::models;
int checks = 0;
void require(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
auto manager() {
    auto basis = a::AnalyticsManager::create_default_basis(a::models::DefaultBasisR,
        a::models::DefaultBasisPhi, a::models::DefaultBasisZ, 10);
    return a::AnalyticsManager::create(std::move(basis));
}
std::vector<a::models::Measurement> measurements(double scale = 1, double transverse = 0, double axial = 5000) {
    std::vector<a::models::Measurement> result;
    for (int side : {-1, 1}) for (int n = 0; n < 6; ++n) {
        const double phi = n * std::numbers::pi / 3;
        result.push_back({{{4 * scale, phi, side * 3.5 * scale}, a::models::CoordinateType::Cylindric},
            {{transverse * std::cos(phi), -transverse * std::sin(phi), axial}, a::models::CoordinateType::Cylindric}});
    }
    return result;
}
std::array<double, 3> cartesian(a::AnalyticsManager& solver, std::array<double, 3> p) {
    const double phi = std::atan2(p[1], p[0]);
    auto result = solver.evaluate_for_point({std::hypot(p[0], p[1]), phi, p[2]});
    if (!result) throw std::runtime_error(result.error());
    const auto b = result->components;
    return {b[0]*std::cos(phi)-b[1]*std::sin(phi), b[0]*std::sin(phi)+b[1]*std::cos(phi), b[2]};
}
void numerical() {
    auto solver = manager();
    const auto synthetic = tpc::system::TPC::create_test_frame();
    auto generated = measurements();
    for (std::size_t i = 0; i < generated.size(); ++i) {
        const auto name = std::string{i < 6 ? "W" : "E"} + std::to_string(i % 6 + 1);
        generated[i].field_components.components = {synthetic.at(name+'R'),synthetic.at(name+'F'),synthetic.at(name+'Z')};
    }
    require(solver->calculate_svd_coefficients(generated).has_value(), "Repository synthetic frame is harmonic");
    require(!solver->export_to_vtk("unused.vtk"), "Export before fit must not throw/succeed");
    for (double scale : {1.0, 100.0, 0.01, 350.0}) {
        auto data = measurements(scale);
        require(solver->calculate_svd_coefficients(data).has_value(), "Unit scaling must preserve rank");
        require(solver->field_quality().rank == 10, "Full ten-mode rank");
        require(std::abs(solver->evaluate_for_point({0, 0, 0})->components[2] - 5000) < 1e-8, "Uniform field preserved");
    }
    require(solver->calculate_field({5,5,5}, 4, 7).has_value(), "Grid evaluation");
    require(solver->field_quality().homogeneous, "Uniform field homogeneous");
    require(!solver->calculate_svd_coefficients({}), "Empty fit rejected");
    require(!solver->get_field_data(), "Failed empty fit invalidates old map");
    auto data = measurements();
    for (auto& sample : data) sample.field_components.components[1] = .5;
    require(!solver->calculate_svd_coefficients(data), "Incompatible azimuthal field rejected by residual");
    data = measurements();
    for (auto& sample : data) sample.point_components.components = {0,0,0};
    require(!solver->calculate_svd_coefficients(data), "Degenerate geometry rejected");
    data = measurements();
    data.front().field_components.components[0] = std::numeric_limits<double>::quiet_NaN();
    require(!solver->calculate_svd_coefficients(data), "NaN rejected");
    data = measurements(1, 5);
    require(solver->calculate_svd_coefficients(data).has_value(), "Tilted uniform fit");
    require(solver->calculate_field({8,8,8}, 4, 7).has_value(), "Tilted field grid");
    require(!solver->field_quality().homogeneous && solver->field_quality().maximum_radial_ratio > 5.2e-4,
        "Tilt exceeds Br/Bz criterion");
    data = measurements(1, 0, .1);
    require(solver->calculate_svd_coefficients(data).has_value(), "Weak field fit");
    require(solver->calculate_field({5,5,5}, 4, 7).has_value(), "Weak field grid");
    require(!solver->field_quality().ratio_defined && !solver->field_quality().homogeneous, "Near-zero Bz cannot pass");
    require(!solver->calculate_field({1024,1024,1024}, 4, 7), "Memory budget enforced");
    std::stop_source cancellation;
    cancellation.request_stop();
    require(!solver->calculate_field({5,5,5}, 4, 7, cancellation.get_token()), "Cancelled grid rejected");
    const auto cancelled_default = solver->calculate_field({128,128,64}, 4, 7, cancellation.get_token());
    require(!cancelled_default && cancelled_default.error() == "Reconstruction cancelled", "Default grid fits memory budget");
    require(!solver->calculate_field({5,5,5}, std::numeric_limits<double>::infinity(), 7), "Infinite geometry rejected");

    // Known source-free harmonic field, exercising all modes, not just constant Bz.
    const std::array<double,10> coefficients{5000,.3,.02,.002,.4,-.2,.01,.02,.04,-.03};
    data = measurements();
    for (auto& sample : data) {
        const auto p = sample.point_components.components;
        auto& b = sample.field_components.components;
        b = {};
        for (int k = 0; k < 10; ++k) {
            b[0] += coefficients[k]*a::models::DefaultBasisR(k,p[0],p[1],p[2]);
            b[1] += coefficients[k]*a::models::DefaultBasisPhi(k,p[0],p[1],p[2]);
            b[2] += coefficients[k]*a::models::DefaultBasisZ(k,p[0],p[1],p[2]);
        }
    }
    require(solver->calculate_svd_coefficients(data).has_value(), "All harmonic modes fit");
    const std::array<double,3> point{1.1,-.8,.6};
    constexpr double h = 1e-3;
    double derivative[3][3]{};
    for (int axis = 0; axis < 3; ++axis) {
        auto plus = point, minus = point;
        plus[axis] += h; minus[axis] -= h;
        const auto bp = cartesian(*solver, plus), bm = cartesian(*solver, minus);
        for (int component = 0; component < 3; ++component)
            derivative[component][axis] = (bp[component]-bm[component])/(2*h);
    }
    require(std::abs(derivative[0][0]+derivative[1][1]+derivative[2][2]) < 1e-7, "Divergence-free correction");
    require(std::abs(derivative[2][1]-derivative[1][2]) < 1e-7
        && std::abs(derivative[0][2]-derivative[2][0]) < 1e-7
        && std::abs(derivative[1][0]-derivative[0][1]) < 1e-7, "Curl-free correction");
}
void telemetry() {
    m::AppSettings settings;
    settings.geometry = {7,4};
    const auto now = std::chrono::steady_clock::now();
    const auto source = std::chrono::system_clock::now();
    m::MeasurementFrame frame;
    for (int side : {-1,1}) for (int n = 1; n <= 6; ++n) {
        m::SensorInfo info;
        info.available = true;
        info.previewable_name = std::string(side == 1 ? "E" : "W") + std::to_string(n);
        info.setName(*m::SensorName::parse(info.previewable_name));
        info.x = 4*std::cos(n*std::numbers::pi/3); info.y = 4*std::sin(n*std::numbers::pi/3); info.z = side*3.5;
        settings.sensors_info.push_back(info);
        for (const char component : {'R','F','Z'})
            frame.emplace(info.previewable_name+component, m::MeasurementSample{component=='Z'?5000.0:0.0,now,source,true,true,true});
    }
    require(m::validatedMeasurements(settings, frame, now, source).has_value(), "36 coherent calibrated channels accepted");
    require(m::measurementsInGauss(settings, frame), "Calibrated frame identified");
    auto uncalibrated = frame;
    uncalibrated.at("E1R").in_gauss = false;
    require(m::validatedMeasurements(settings, uncalibrated, now, source).has_value(), "Missing calibration does not block preliminary calculation");
    require(!m::measurementsInGauss(settings, uncalibrated), "Preliminary frame not mislabeled as calibrated");
    for (int failure = 0; failure < 6; ++failure) {
        auto invalid = frame;
        auto& sample = invalid.at("E1R");
        switch (failure) {
            case 0: invalid.erase("E1R"); break;
            case 1: sample.good = false; break;
            case 2: sample.value = std::numeric_limits<double>::quiet_NaN(); break;
            case 3: sample.has_source_time = false; break;
            case 4: sample.received_at -= std::chrono::seconds{4}; break;
            case 5: sample.source_time -= std::chrono::milliseconds{1500}; break;
        }
        require(!m::validatedMeasurements(settings, invalid, now, source), "Invalid telemetry must block fit");
    }
    settings.sensors_info[1] = settings.sensors_info[0];
    require(!m::validatedMeasurements(settings, frame, now, source), "Duplicate sensor rejected");
}
void reference() {
    auto map = m::ReferenceFieldMap::load(TPC_TEST_REFERENCE_PATH);
    require(map.has_value(), "Reference JSON loaded");
    const auto value = (*map)->evaluate({1,2,3});
    require(value && std::abs((*value)[0]-.2)<1e-10 && std::abs((*value)[1]-.8)<1e-10
        && std::abs((*value)[2]-5001.8)<1e-10, "Trilinear interpolation respects XYZ ordering");
    require(!(*map)->evaluate({6,0,0}), "No reference extrapolation");
    auto solver = manager();
    solver->set_reference_field([reference=*map](auto p) -> std::expected<a::models::FieldComponents,std::string> {
        auto b = reference->evaluate(p);
        if (!b) return std::unexpected(b.error());
        return a::models::FieldComponents{*b, a::models::CoordinateType::Cartesian};
    });
    auto data = measurements();
    for (auto& sample : data) {
        const auto p = sample.point_components.components;
        const auto b = *(*map)->evaluate({p[0]*std::cos(p[1]),p[0]*std::sin(p[1]),p[2]});
        sample.field_components.components = {b[0]*std::cos(p[1])+b[1]*std::sin(p[1]),
            -b[0]*std::sin(p[1])+b[1]*std::cos(p[1]),b[2]+1};
    }
    require(solver->calculate_svd_coefficients(data).has_value(), "Fit residual relative to reference");
    const auto restored = cartesian(*solver,{1,2,3});
    require(std::abs(restored[0]-.2)<1e-8 && std::abs(restored[1]-.8)<1e-8
        && std::abs(restored[2]-5002.8)<1e-8, "Reference plus correction, not replacement");
    require(solver->field_quality().reference_corrected, "Reference provenance recorded");
}
void warningsWorkflow() {
    a::models::ReconstructionLimits limits;
    limits.warnings_only = true;
    auto solver = manager();
    auto data = measurements();
    for (auto& sample : data) sample.field_components.components[1] = .5;
    require(solver->calculate_svd_coefficients(data, limits.svd_threshold, limits).has_value(), "Residual warning permits calculation");
    require(!solver->field_quality().fit_within_limit, "Residual warning retained");
    for (auto& sample : data) sample.point_components.components = {0,0,0};
    require(solver->calculate_svd_coefficients(data, limits.svd_threshold, limits).has_value(), "Zero positions permit minimum-norm preview");
    require(solver->field_quality().rank < 10 && solver->field_quality().geometry_degenerate, "Degenerate geometry warning retained");
    require(solver->calculate_field({5,5,5},4,7).has_value(), "Warning does not block grid");
    require(solver->evaluate_field_slice(a::models::SliceDirection::Z,0,{16,16},4,7).has_value(), "Warning does not block slices");
    const auto file = std::filesystem::temp_directory_path()
        / ("tpc-warning-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".vtk");
    const auto exported = solver->export_to_vtk(file.string());
    require(exported.has_value(), "Warning does not block VTK export");
    require(std::filesystem::file_size(file) > 0, "VTK file written");
    std::filesystem::remove(file);

    m::AppSettings settings;
    settings.geometry = {7,4};
    m::SensorInfo info;
        info.available = true;
    info.setName(*m::SensorName::parse("E1"));
    settings.sensors_info.push_back(info);
    m::MeasurementFrame frame;
    frame.emplace("E1Z",m::MeasurementSample{30,{},{},false,false,false});
    auto prepared = m::preparePreliminaryMeasurements(settings,frame);
    require(prepared.has_value(), "Quality/position/count warnings permit preview preparation");
    require(!prepared->warning.empty() && prepared->sensors[0].values[2] == 30,
        "Received values retained and missing-value warning shown");
    require(!m::preparePreliminaryMeasurements(settings,{}), "No data remains a technical error");
}
void millimetreSettings() {
    namespace s = tpc_slint::services;
    const auto directory = std::filesystem::temp_directory_path()
        / ("tpc-mm-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{directory};
    s::EventDispatcher events;
    s::FileWorker file{directory / "settings.json"};
    s::SettingsHolderService settings{events, file};
    require(settings.loadSettings().has_value(), "Measured defaults load");
    auto current = settings.currentSettings();
    require(current.coordinate_unit == "mm" && current.geometry.length == 4058
        && current.geometry.radius == 1408.569 && current.sensors_info.size() == 12,
        "Default volume and sensor positions use millimetres");
    const auto w2 = std::ranges::find(current.sensors_info, std::string{"W2"}, &m::SensorInfo::previewable_name);
    require(w2 != current.sensors_info.end() && std::abs(w2->x - 1219.856) < .001
        && std::abs(w2->y - 704.285) < .001 && w2->z == 2029,
        "Measured W2 coordinates and positive W side retained");
    const auto e1 = std::ranges::find(current.sensors_info, std::string{"E1"}, &m::SensorInfo::previewable_name);
    require(e1 != current.sensors_info.end() && e1->x == 0 && e1->z == -2029,
        "Measured E side has negative Z");
    auto solver = manager();
    std::vector<a::models::Measurement> measured;
    for (const auto& sensor : current.sensors_info) {
        measured.push_back({{{sensor.x, sensor.y, sensor.z}, a::models::CoordinateType::Cartesian},
            {{0, 0, 5000}, a::models::CoordinateType::Cylindric}});
    }
    require(solver->calculate_svd_coefficients(measured).has_value(),
        "Reconstruction accepts actual sensor coordinates in millimetres");
    const auto restored = cartesian(*solver, {1000, 200, 1800});
    require(std::abs(restored[0]) < 1e-7 && std::abs(restored[1]) < 1e-7
        && std::abs(restored[2] - 5000) < 1e-7, "Field preserved at millimetre-scale positions");
    auto slice = solver->evaluate_field_slice(a::models::SliceDirection::Z, 2029, {8,8},
        current.geometry.radius, current.geometry.length);
    require(slice.has_value() && slice->horizontal_bounds[1] == 1408.569,
        "Slice coordinates and bounds remain in millimetres");

    nlohmann::json legacy(current);
    legacy.erase("coordinate_unit");
    legacy["geometry"] = {{"length", 7.0}, {"radius", 4.0}};
    legacy["sensors_info"][0]["x"] = 4;
    require(file.writeSettings(legacy.dump()).has_value() && settings.loadSettings().has_value(),
        "Legacy example settings migrate");
    current = settings.currentSettings();
    require(current.coordinate_unit == "mm" && current.geometry.length == 4058
        && current.sensors_info[0].x == 0 && current.sensors_info[0].z == 2029,
        "Legacy example layout replaced by measured layout");
    const auto saved = *file.loadSettings();
    require(settings.loadSettings().has_value() && *file.loadSettings() == saved,
        "Reload does not multiply millimetres again");

    legacy["coordinate_unit"] = "cm";
    legacy["geometry"] = {{"length", 500.0}, {"radius", 200.0}};
    legacy["sensors_info"].push_back({{"previewable_name", "E7"}, {"x", 12.0}, {"y", 3.0}, {"z", -4.0}});
    require(file.writeSettings(legacy.dump()).has_value() && settings.loadSettings().has_value(),
        "Explicit centimetre settings migrate");
    current = settings.currentSettings();
    require(current.geometry.length == 5000 && current.geometry.radius == 2000
        && current.sensors_info.back().x == 120 && current.sensors_info.back().z == -40,
        "Custom volume and channels absent from measured file convert cm to mm");
    legacy["coordinate_unit"] = "m";
    const auto invalid = legacy.dump();
    require(file.writeSettings(invalid).has_value() && !settings.loadSettings()
        && *file.loadSettings() == invalid, "Unsupported units rejected without overwriting settings");

    nlohmann::json map;
    { std::ifstream input{TPC_TEST_REFERENCE_PATH}; input >> map; }
    const auto mmPath = directory / "mm-map.json";
    const auto cmPath = directory / "cm-map.json";
    s::FileWorker mmFile{mmPath}, cmFile{cmPath};
    require(mmFile.writeSettings(map.dump()).has_value(), "Millimetre reference fixture written");
    map["coordinate_unit"] = "cm";
    for (const auto axis : {"x", "y", "z"})
        for (auto& value : map[axis]) value = value.get<double>() / 10.0;
    require(cmFile.writeSettings(map.dump()).has_value(), "Centimetre reference fixture written");
    auto mm = m::ReferenceFieldMap::load(mmPath);
    auto cm = m::ReferenceFieldMap::load(cmPath);
    require(mm && cm, "Both reference units load");
    const auto bmm = (*mm)->evaluate({1,2,3});
    const auto bcm = (*cm)->evaluate({1,2,3});
    require(bmm && bcm && *bmm == *bcm, "Centimetre reference axes convert to identical millimetre evaluations");
}
void fieldProbe() {
    tpc_slint::services::RenderedFieldSlice slice;
    slice.width = 3;
    slice.height = 3;
    slice.axis = 2;
    slice.coordinate = 100;
    slice.u_range = {-20,20};
    slice.v_range = {-30,30};
    slice.valid.assign(9, 1);
    for (int n = 0; n < 9; ++n) slice.field.insert(slice.field.end(), {double(n), .5, 5000});
    // Square image in a wide viewport: centred with 50px side margins.
    require(!slice.probe(49,50,200,100) && !slice.probe(150,50,200,100),
        "Hover ignores centred image margins");
    auto centre = slice.probe(100,50,200,100);
    require(centre && centre->position_mm == std::array<double,3>{0,0,100}
        && centre->cartesian_field[0] == 4, "Hover matches the displayed centre grid sample");
    auto top = slice.probe(51,1,200,100);
    require(top && top->position_mm == std::array<double,3>{-20,30,100},
        "Hover maps image top to positive vertical coordinate");
    slice.valid[0] = 0;
    require(!slice.probe(51,1,200,100), "Outside-cylinder mask suppresses hover values");
    slice.axis = 0;
    centre = slice.probe(100,50,200,100);
    require(centre && centre->position_mm == std::array<double,3>{100,0,0}, "X slice maps Y/Z correctly");
    slice.axis = 1;
    centre = slice.probe(100,50,200,100);
    require(centre && centre->position_mm == std::array<double,3>{0,100,0}, "Y slice maps X/Z correctly");
    require(!slice.probe(-1,50,200,100) && !slice.probe(100,50,0,100),
        "Invalid hover positions and viewport dimensions ignored");
    slice.field[12] = std::numeric_limits<double>::quiet_NaN();
    require(!slice.probe(100,50,200,100), "Non-finite field samples never displayed");
}
void fieldOverlays() {
    tpc_slint::services::RenderedFieldSlice slice;
    slice.width = slice.height = 3;
    slice.axis = 2;
    slice.coordinate = 100;
    slice.u_range = {-20,20};
    slice.v_range = {-30,30};
    slice.valid.assign(9, 1);
    slice.field.assign(27, 1);
    std::vector<m::Sensor> sensors{
        {*m::SensorName::parse("W1"), {0, 30, 100}},
        {*m::SensorName::parse("E1"), {0, 30, -100}},
        {*m::SensorName::parse("W2"), {20, 0, 100}},
        {*m::SensorName::parse("W3"), {21, 0, 100}}
    };
    auto markers = tpc_slint::viewmodels::fieldSensorMarkers(slice, sensors);
    require(markers.size() == 2 && markers[0].in_slice,
        "Coincident Hall projections share a point highlighted when a sensor is in the slice");
    auto sample = slice.probe(markers[0].u * 300, markers[0].v * 300, 300, 300);
    require(sample && sample->position_mm == sensors[0].position,
        "Hall marker maps to the same grid sample as the field probe, including image edges");
    // A map enlarged to 600px, translated by (-150,-100) in a 300px viewport.
    const double pointer_x = markers[1].u * 600 - 150;
    const double pointer_y = markers[1].v * 600 - 100;
    sample = slice.probe(pointer_x + 150, pointer_y + 100, 600, 600);
    require(sample && sample->position_mm == sensors[2].position,
        "Probe remains aligned with the sensor after inverse zoom/pan transformation");
    slice.axis = 0;
    slice.coordinate = 0;
    slice.u_range = {-30,30};
    slice.v_range = {-100,100};
    markers = tpc_slint::viewmodels::fieldSensorMarkers(slice, sensors);
    require(markers.size() == 3 && markers[0].in_slice && markers[0].u > .8 && markers[0].v > .8,
        "X slices project Hall positions onto Z/Y with positive Z to the right");
    slice.axis = 1;
    markers = tpc_slint::viewmodels::fieldSensorMarkers(slice, sensors);
    require(markers.size() == 4 && !markers[0].in_slice && markers[2].in_slice,
        "Y slices project onto X/Z and test the omitted Y coordinate for slice membership");
    sensors[0].position[0] = std::numeric_limits<double>::quiet_NaN();
    require(tpc_slint::viewmodels::fieldSensorMarkers(slice, sensors).size() == 3, "Invalid sensor coordinates are not rendered");
}
void fieldAntialiasing() {
    namespace services = tpc_slint::services;
    m::NumericFieldSlice numeric;
    numeric.grid = {33,33};
    numeric.horizontal_bounds = numeric.vertical_bounds = {-16,16};
    numeric.field.assign(33*33*3, 0);
    numeric.valid.assign(33*33, 0);
    for (int row = 0; row < 33; ++row) for (int column = 0; column < 33; ++column) {
        const auto pixel = row*33+column;
        numeric.field[pixel*3+2] = 5000;
        numeric.valid[pixel] = std::hypot(column-16., row-16.) <= 16;
    }
    const auto rendered = services::renderFieldSlice(numeric, 2, 0, 16);
    require(rendered && rendered->field == numeric.field && rendered->valid == numeric.valid,
        "Antialiasing preserves the original numeric field and probe mask");
    const auto alpha = [&](int column, int row) { return rendered->rgba[(row*33+column)*4+3]; };
    require(alpha(16,16) == 255 && alpha(0,0) == 0 && alpha(16,0) > 0 && alpha(16,0) < 255,
        "Circle coverage has an opaque centre, transparent exterior and antialiased rim");
    require(rendered->minimum_value == 5000 && rendered->maximum_value == 5000,
        "Coverage sampling does not alter field extrema");
    bool partial_outside_node = false;
    for (int row = 0; row < 33; ++row) for (int column = 0; column < 33; ++column) {
        const auto pixel = row*33+column;
        if (!alpha(column,row)) continue;
        for (int channel = 0; channel < 3; ++channel)
            require(rendered->rgba[pixel*4+channel] == rendered->rgba[(16*33+16)*4+channel],
                "Partially covered pixels retain field colour without a black alpha halo");
        if (!numeric.valid[pixel]) {
            partial_outside_node = true;
            require(!rendered->probe(column+.5,row+.5,33,33),
                "A partially visible edge does not invent a measurement outside the probe mask");
        }
    }
    require(partial_outside_node, "Subpixel circle coverage smooths the binary sample mask");
    for (int row = 0; row < 33; ++row) for (int column = 0; column < 33; ++column) {
        numeric.valid[row*33+column] = 1;
        numeric.field[(row*33+column)*3+2] = 5000 + column;
    }
    const auto bands = services::renderFieldSlice(numeric, 0, 0, 16);
    const auto red = [&](int column) { return bands->rgba[(column*33+16)*4]; };
    require(bands && red(3) > 30 && red(3) < 45 && red(2) > 60,
        "Isoline pixels mix subpixel line and background coverage instead of hard band edges");
    require(bands->field == numeric.field && bands->valid == numeric.valid,
        "Smoothed isolines preserve original field components");
    std::stop_source cancelled;
    cancelled.request_stop();
    require(!services::renderFieldSlice(numeric, 0, 0, 16, cancelled.get_token()),
        "Antialiasing honours render cancellation");
    numeric.valid.assign(33*33, 0);
    require(!services::renderFieldSlice(numeric, 0, 0, 16), "Empty slices are not rasterized");
}
void horizontalLongitudinalSlices() {
    namespace services = tpc_slint::services;
    m::NumericFieldSlice numeric;
    numeric.grid = {3,5};
    numeric.horizontal_bounds = {-2,2};
    numeric.vertical_bounds = {-6,6};
    numeric.valid.assign(15, 1);
    for (int row = 0; row < 5; ++row) for (int column = 0; column < 3; ++column)
        numeric.field.insert(numeric.field.end(), {1., 2., 100.+10*row+column});
    const auto upright = services::renderFieldSlice(numeric, 2, 3, 100);
    require(upright && upright->displayWidth() == 3 && upright->displayHeight() == 5,
        "Z slices retain their original image orientation");
    for (const int axis : {0,1}) {
        const auto horizontal = services::renderFieldSlice(numeric, axis, 3, 100);
        require(horizontal && horizontal->displayWidth() == 5 && horizontal->displayHeight() == 3,
            "Longitudinal images swap dimensions to place cylinder length horizontally");
        require(horizontal->field == numeric.field && horizontal->valid == numeric.valid,
            "Rotating the image preserves field data and numeric sample order");
        for (int row = 0; row < 5; ++row) for (int column = 0; column < 3; ++column) {
            const auto source_pixel = row*3+column;
            const auto rotated_pixel = column*5+4-row;
            for (int channel = 0; channel < 4; ++channel)
                require(horizontal->rgba[rotated_pixel*4+channel] == upright->rgba[source_pixel*4+channel],
                    "All RGBA samples rotate clockwise, including antialias coverage");
            const auto probe = horizontal->probe((4-row+.5)*100, (column+.5)*100, 500, 300);
            const double transverse = -2.+2*column, z = 6.-3*row;
            const std::array<double,3> position = axis == 0 ? std::array<double,3>{3,transverse,z}
                                                          : std::array<double,3>{transverse,3,z};
            require(probe && probe->position_mm == position && probe->cartesian_field[2] == 100.+10*row+column,
                "Hover on each rotated pixel retrieves its original field sample and XYZ coordinates");
        }
        require(!horizontal->probe(149,150,800,300) && !horizontal->probe(650,150,800,300),
            "Rotated image probes still exclude centred viewport margins");
        std::vector<m::Sensor> sensors{{*m::SensorName::parse("W1"), axis == 0
            ? std::array<double,3>{3,2,-6} : std::array<double,3>{2,3,-6}}};
        const auto markers = tpc_slint::viewmodels::fieldSensorMarkers(*horizontal, sensors);
        require(markers.size() == 1 && markers[0].in_slice && markers[0].u < .2 && markers[0].v > .8,
            "Longitudinal sensor markers place negative Z left and positive transverse coordinates down");
        const auto sensor_probe = horizontal->probe(markers[0].u*500, markers[0].v*300, 500, 300);
        require(sensor_probe && sensor_probe->position_mm == sensors[0].position,
            "Projected sensor points and hover remain aligned after rotation");
        const auto grid = tpc_slint::viewmodels::fieldGrid(*horizontal, {750,300,0,0,750,300});
        const auto negative = std::ranges::find_if(grid.vertical,[](const auto& tick) {
            return tick.major && tick.label.starts_with("-");
        });
        require(negative != grid.vertical.end() && negative->position < .5,
            "Longitudinal vertical axis divisions increase downward along the rotated transverse axis");
    }
}
void movingSliceFrame() {
    auto solver = manager();
    require(solver->calculate_svd_coefficients(measurements()).has_value(), "Moving-slice fixture fits");
    for (const auto direction : {a::models::SliceDirection::X, a::models::SliceDirection::Y}) {
        const int axis = direction == a::models::SliceDirection::X ? 0 : 1;
        const auto tight = solver->evaluate_field_slice(direction, 2, {65,97}, 4, 7);
        require(tight && tight->horizontal_bounds[1] < 4,
            "Existing API callers retain tight slice bounds by default");
        for (const double coordinate : {-4.,-3.8,-2.,0.,2.,3.8,4.}) {
            const auto full = solver->evaluate_field_slice(direction, coordinate, {65,97}, 4, 7, {},
                                                          a::models::SliceFraming::FullVolume);
            require(full && full->horizontal_bounds == std::array<double,2>{-4,4}
                && full->vertical_bounds == std::array<double,2>{-3.5,3.5},
                "Moving longitudinal slices retain the full diameter and cylinder length");
            const auto valid_nodes = std::ranges::count(full->valid, std::uint8_t{1});
            require(coordinate == 0 ? valid_nodes == 65*97 : valid_nodes < 65*97,
                "Physical cross-section changes through the validity mask, not the image frame");
            m::NumericFieldSlice numeric{full->grid, full->horizontal_bounds, full->vertical_bounds, full->field, full->valid};
            const auto image = tpc_slint::services::renderFieldSlice(std::move(numeric), axis, coordinate, 4);
            require(image && image->displayWidth() == 97 && image->displayHeight() == 65,
                "Image aspect ratio and pixel scale remain fixed at every slice position");
            const auto left = image->probe(.5,32.5,97,65);
            const auto right = image->probe(96.5,32.5,97,65);
            require(left && right && left->position_mm[2] == -3.5 && right->position_mm[2] == 3.5,
                "Both cylinder ends stay at the same screen coordinates while the slice moves");
            require(left->position_mm[axis] == coordinate && right->position_mm[axis] == coordinate,
                "Probe positions still track the selected slice coordinate");
            if (std::abs(coordinate) == 3.8) {
                require(!image->probe(48.5,.5,97,65) && image->rgba[(0*97+48)*4+3] == 0,
                    "Points outside the smaller chord are transparent and cannot be probed");
                require(image->rgba[(32*97+48)*4+3] == 255,
                    "The remaining physical cross-section stays fully rendered");
            }
            const std::vector<m::Sensor> sensors{{*m::SensorName::parse("E1"), {0,0,-3.5}}};
            const auto markers = tpc_slint::viewmodels::fieldSensorMarkers(*image, sensors);
            require(markers.size() == 1 && std::abs(markers[0].u - .5/97) < 1e-12 && markers[0].v == .5,
                "Hall projections retain their screen positions across the entire slice sweep");
            require(image->horizontalRange() == std::array<double,2>{-3.5,3.5}
                && image->verticalRange() == std::array<double,2>{-4,4},
                "Rulers retain the same coordinate frame across slice positions");
        }
    }
}
void adaptiveFieldGrid() {
    namespace vm = tpc_slint::viewmodels;
    tpc_slint::services::RenderedFieldSlice slice;
    slice.width=281; slice.height=407;
    slice.u_range={-1408.569,1408.569}; slice.v_range={-2029,2029};
    for (int axis : {0,1,2}) {
        slice.axis=axis;
        const double physical_width=(slice.horizontalRange()[1]-slice.horizontalRange()[0])*slice.displayWidth()/(slice.displayWidth()-1);
        const double physical_height=(slice.verticalRange()[1]-slice.verticalRange()[0])*slice.displayHeight()/(slice.displayHeight()-1);
        vm::FieldViewport view{1200,800,0,0,physical_width/5,physical_height/5};
        view.map_x=(view.width-view.map_width)/2; view.map_y=(view.height-view.map_height)/2;
        const auto grid=vm::fieldGrid(slice,view);
        require(grid.major_step==500,"Both plot axes use one physical millimetre step");
        const auto major_spacing = [](const auto& ticks,double size) {
            std::vector<double> major;
            for(const auto& tick : ticks) if(tick.major) major.push_back(tick.position*size);
            require(major.size()>1,"Visible axes contain multiple major divisions");
            return std::abs(major[1]-major[0]);
        };
        const double dx=major_spacing(grid.horizontal,view.map_width),dy=major_spacing(grid.vertical,view.map_height);
        require(std::abs(dx-dy)<1e-9 && std::abs(dx-100)<1e-9,
            "Equal millimetre distances form square grid cells on all slice planes");
        require(std::ranges::any_of(grid.horizontal,[](const auto& tick){return tick.position<0 || tick.position>1;}),
            "Plot grid covers visible space beyond the field image");
        require(std::ranges::all_of(grid.horizontal,[](const auto& tick){return tick.major || tick.label.empty();}),
            "Only major divisions receive labels, avoiding overlapping minor numbers");
        view.map_x+=137; view.map_y-=83;
        const auto panned=vm::fieldGrid(slice,view);
        require(panned.major_step==grid.major_step,"Panning does not change grid density");
        const auto origin=[](const auto& ticks) {
            const auto zero=std::ranges::find_if(ticks,[](const auto& tick){return tick.zero;});
            require(zero!=ticks.end() && zero->label=="0","Visible origin is labelled without negative zero");
            return zero->position;
        };
        require(origin(panned.horizontal)==origin(grid.horizontal) && origin(panned.vertical)==origin(grid.vertical),
            "Axes and grid retain the same physical origin under camera translation");
        view.map_width*=4;view.map_height*=4;
        view.map_x=(view.width-view.map_width)/2;view.map_y=(view.height-view.map_height)/2;
        const auto zoomed=vm::fieldGrid(slice,view);
        require(zoomed.major_step<grid.major_step && major_spacing(zoomed.horizontal,view.map_width)>=74,
            "Zoom refines the grid while keeping axis labels far enough apart");
        require(zoomed.horizontal.size()<512 && zoomed.vertical.size()<512,"Grid model size is bounded");
    }
    require(vm::fieldGrid(slice,{}).horizontal.empty(),"Uninitialised viewports do not emit invalid grid lines");
    const auto colors=vm::fieldColorTicks(5000.,5000.001);
    require(colors.size()==6 && colors.front().position==0 && colors.back().position==1,
        "Colour scale spans the complete range of the displayed field");
    for (std::size_t i=1;i<colors.size();++i) {
        require(colors[i].label!=colors[i-1].label,"Small field variations retain distinct colour-scale labels");
        require(std::abs(std::stod(colors[i].label)-(5000.+i*.0002))<1e-9,
            "Colour-scale labels describe real field values");
    }
    const auto uniform=vm::fieldColorTicks(12.,12.);
    require(uniform.size()==1 && uniform[0].position==.5 && uniform[0].label=="12",
        "A uniform field has one colour and one value on the colour scale");
    require(vm::fieldColorTicks(1,0).empty(),"Invalid colour ranges do not produce ticks");
}
int main() {
    try { numerical(); telemetry(); reference(); warningsWorkflow(); millimetreSettings(); fieldProbe(); fieldOverlays(); fieldAntialiasing(); horizontalLongitudinalSlices(); movingSliceFrame(); adaptiveFieldGrid(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << checks << " scientific checks passed\n";
}
