#include "tpc/analytics/analytics_manager/analytics_manager.hpp"
#include "tpc/analytics/models/basis_models.hpp"
#include "tpc/tpc.hpp"
#include "models/measurement_quality.hpp"
#include "models/reference_field_map.hpp"
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
    for (double scale : {1.0, 100.0, 0.01}) {
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
int main() {
    try { numerical(); telemetry(); reference(); warningsWorkflow(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << checks << " scientific checks passed\n";
}
