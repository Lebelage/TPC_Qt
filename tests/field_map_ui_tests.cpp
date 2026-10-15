#include <cmath>

#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <slint-platform.h>
#include <stdexcept>
#include <thread>
#include <vector>

#include "app.h"
#include "application/application_context.hpp"
#include "services/field_slice/rendered_field_slice.hpp"
#include "ui/application_view_binding.hpp"
#include "viewmodels/application_view_model.hpp"
#include "viewmodels/field_map_overlay.hpp"

namespace {
namespace vm = tpc_slint::viewmodels;
std::mutex ui_tasks_mutex;
std::deque<slint::platform::Platform::Task> ui_tasks;
void drainUiTasks() {
    for (;;) {
        std::deque<slint::platform::Platform::Task> tasks;
        {
            std::scoped_lock lock{ui_tasks_mutex};
            tasks.swap(ui_tasks);
        }
        if (tasks.empty())
            return;
        for (auto& task : tasks)
            std::move(task).run();
    }
}
class TestWindow final : public slint::platform::WindowAdapter {
    slint::platform::SoftwareRenderer renderer_{slint::platform::SoftwareRenderer::RepaintBufferType::NewBuffer};
    slint::PhysicalSize size_ = slint::PhysicalSize({900, 740});

public:
    slint::PhysicalSize size() override {
        return size_;
    }
    slint::platform::AbstractRenderer& renderer() override {
        return renderer_;
    }
    void set_size(slint::PhysicalSize size) override {
        size_ = size;
        window().dispatch_resize_event(slint::LogicalSize({float(size.width), float(size.height)}));
    }
    std::vector<slint::Rgb8Pixel> render() {
        drainUiTasks();
        slint::platform::update_timers_and_animations();
        std::vector<slint::Rgb8Pixel> buffer(size_.width * size_.height);
        renderer_.render(buffer, size_.width);
        return buffer;
    }
};
TestWindow* test_window{};
class TestPlatform final : public slint::platform::Platform {
    void run_in_event_loop(Task task) override {
        std::scoped_lock lock{ui_tasks_mutex};
        ui_tasks.push_back(std::move(task));
    }
    std::unique_ptr<slint::platform::WindowAdapter> create_window_adapter() override {
        auto window = std::make_unique<TestWindow>();
        test_window = window.get();
        return window;
    }
};
void require(bool result, const char* message) {
    if (!result)
        throw std::runtime_error(message);
}
void fieldMapInteraction() {
    slint::platform::set_platform(std::make_unique<TestPlatform>());
    auto ui = FieldWindow::create();
    auto& window = ui->window();
    vm::FieldViewport view;
    tpc_slint::services::RenderedFieldSlice slice;
    slice.width = slice.height = 401;
    slice.u_range = slice.v_range = {-1408.569, 1408.569};
    const auto tick_model = [](const auto& ticks) {
        std::vector<MapTick> rows;
        for (const auto& tick : ticks) {
            MapTick row;
            row.position = tick.position;
            row.label = slint::SharedString{tick.label};
            row.zero = tick.zero;
            row.major = tick.major;
            rows.push_back(row);
        }
        return std::make_shared<slint::VectorModel<MapTick>>(std::move(rows));
    };
    ui->on_map_view_changed([&](float w, float h, float x, float y, float mw, float mh) {
        view = {w, h, x, y, mw, mh};
        const auto grid = vm::fieldGrid(slice, view);
        ui->set_horizontal_ticks(tick_model(grid.horizontal));
        ui->set_vertical_ticks(tick_model(grid.vertical));
        ui->set_grid_step(grid.major_step);
    });
    float zoom = 1, x = 0, y = 0, width = 0, height = 0;
    int probes = 0;
    ui->on_map_zoom_changed([&](float value) {
        zoom = value;
    });
    ui->on_inspect_field([&](float px, float py, float w, float h) {
        x = px;
        y = py;
        width = w;
        height = h;
        ++probes;
        return slint::SharedString{};
    });
    window.set_size(slint::PhysicalSize({900, 740}));
    ui->set_available(true);
    ui->set_show_grid(true);
    ui->set_image_width(401);
    ui->set_image_height(401);
    ui->set_horizontal_minimum(-1408.569);
    ui->set_horizontal_maximum(1408.569);
    ui->set_vertical_minimum(-1408.569);
    ui->set_vertical_maximum(1408.569);
    ui->set_color_ticks(tick_model(vm::fieldColorTicks(4000, 5000)));
    ui->show();
    test_window->render();
    ui->invoke_refresh_plot();
    const auto point = slint::LogicalPosition({450, 400});
    window.dispatch_pointer_move_event(point);
    require(probes > 0 && width > 0 && height > 0, "Map hover reaches the field probe");
    const double canvas_x = point.x - x - view.map_x, canvas_y = point.y - y - view.map_y;
    const auto pixel = [&](const auto& pixels, double px, double py) {
        const auto ix = std::lround(px), iy = std::lround(py);
        require(
            ix >= 0 && iy >= 0 && ix < test_window->size().width && iy < test_window->size().height,
            "Grid test samples lie inside the window"
        );
        return pixels[iy * test_window->size().width + ix];
    };
    const auto check_grid = [&] {
        test_window->render();
        ui->invoke_refresh_plot();
        const auto pixels = test_window->render();
        const double gx = canvas_x + std::round(view.map_x + .5 * view.map_width);
        const double gy = canvas_y + std::round(view.map_y + .5 * view.map_height);
        require(
            pixel(pixels, gx, canvas_y + 8).r < 220 && pixel(pixels, canvas_x + 8, gy).r < 220,
            "Origin grid lines span the plot, including the image margins"
        );
        require(
            pixel(pixels, gx, canvas_y + view.height + 2).r < 150 && pixel(pixels, canvas_x - 2, gy).r < 150,
            "Bottom and left axis ticks align with their plot grid lines"
        );
        const auto high = pixel(pixels, canvas_x + view.width + 32, canvas_y + 3);
        const auto low = pixel(pixels, canvas_x + view.width + 32, canvas_y + view.height - 4);
        require(
            high.r > 200 && high.g > 180 && low.r < 100 && low.g < 50,
            "Colour bar shows maximum yellow at the top and minimum violet at the bottom"
        );
    };
    check_grid();
    ui->set_show_grid(false);
    const auto hidden = test_window->render();
    const double zero_y = canvas_y + std::round(view.map_y + .5 * view.map_height);
    require(pixel(hidden, canvas_x + 8, zero_y).r > 240, "Grid toggle removes plot lines");
    require(pixel(hidden, canvas_x - 2, zero_y).r < 150, "Axis ticks remain visible with the grid disabled");
    ui->set_color_ticks(tick_model(vm::fieldColorTicks(5000, 5000)));
    const auto uniform = test_window->render();
    const auto top = pixel(uniform, canvas_x + view.width + 32, canvas_y + 3);
    const auto bottom = pixel(uniform, canvas_x + view.width + 32, canvas_y + view.height - 4);
    require(
        top.r == bottom.r && top.g == bottom.g && top.b == bottom.b && top.g > top.r,
        "A constant field uses the same midpoint colour across the complete colour bar"
    );
    ui->set_color_ticks(tick_model(vm::fieldColorTicks(4000, 5000)));
    ui->set_show_grid(true);
    const float before_x = x / width, before_y = y / height;
    window.dispatch_pointer_scroll_event(point, 0, 347);
    test_window->render();
    window.dispatch_pointer_move_event(slint::LogicalPosition({451, 400}));
    require(zoom > 1.9 && zoom < 2.1, "Mouse wheel enlarges the map");
    require(
        std::abs(x / width - before_x) < .005 && std::abs(y / height - before_y) < .005,
        "Zoom preserves the coordinate under the cursor"
    );
    const float probe_x = x, probe_y = y;
    window.dispatch_pointer_press_event(point, slint::PointerEventButton::Left);
    window.dispatch_pointer_move_event(slint::LogicalPosition({490, 420}));
    window.dispatch_pointer_release_event(slint::LogicalPosition({490, 420}), slint::PointerEventButton::Left);
    window.dispatch_pointer_move_event(slint::LogicalPosition({491, 420}));
    require(std::abs(x - probe_x) < 2 && std::abs(y - probe_y) < 2, "Probe and image share the same drag translation");
    check_grid();
    const auto click = [&](slint::LogicalPosition position) {
        window.dispatch_pointer_move_event(position);
        window.dispatch_pointer_press_event(position, slint::PointerEventButton::Left);
        window.dispatch_pointer_release_event(position, slint::PointerEventButton::Left);
    };
    window.dispatch_pointer_scroll_event(point, 0, 10000);
    require(zoom == 8, "Zoom is limited to 800 percent");
    window.dispatch_pointer_scroll_event(point, 0, -10000);
    require(zoom == 1, "Zoom is limited to 100 percent");
    ui->invoke_fit_view();
    test_window->render();
    window.dispatch_pointer_move_event(point);
    require(
        std::abs(x / width - before_x) < .005 && std::abs(y / height - before_y) < .005,
        "Fit restores scale and camera centre"
    );
    ui->invoke_zoom_in();
    require(zoom == 1.25, "Plus button enlarges the map");
    ui->invoke_fit_view();
    require(zoom == 1, "Fit button restores the original scale");
    // The plot can also be panned at 100%, using the middle button.
    window.dispatch_pointer_press_event(point, slint::PointerEventButton::Middle);
    window.dispatch_pointer_move_event(slint::LogicalPosition({480, 415}));
    window.dispatch_pointer_release_event(slint::LogicalPosition({480, 415}), slint::PointerEventButton::Middle);
    test_window->render();
    require(std::abs(view.map_x - (view.width - view.map_width) / 2) > 20, "Middle-button panning works at fit scale");
    const auto centre = [&] {
        window.dispatch_pointer_move_event(
            slint::LogicalPosition({float(canvas_x + view.width / 2), float(canvas_y + view.height / 2)})
        );
        return std::array{x / width, y / height};
    };
    const auto centre_before = centre();
    window.set_size(slint::PhysicalSize({1920, 1080}));
    test_window->render();
    const auto centre_after = centre();
    require(
        std::abs(centre_before[0] - centre_after[0]) < .005 && std::abs(centre_before[1] - centre_after[1]) < .005,
        "Resizing preserves the physical coordinate at the camera centre"
    );
    check_grid();
    ui->invoke_fit_view();
    window.dispatch_pointer_move_event(point);
    window.dispatch_pointer_scroll_event(point, 0, 347);
    click(point);
    click(point);
    require(zoom == 1, "Double-click fits the complete field");
    ui->set_loading(true);
    const int old_probes = probes;
    window.dispatch_pointer_move_event(slint::LogicalPosition({451, 401}));
    require(probes == old_probes, "Loading suppresses stale field probes");
    ui->set_available(false);
    window.dispatch_pointer_scroll_event(point, 0, 347);
    require(zoom == 1, "Unavailable maps ignore zoom");
    ui->set_available(true);
    ui->set_loading(false);
    slice.width = 281;
    slice.height = 407;
    slice.axis = 2;
    slice.v_range = {-2029, 2029};
    ui->set_horizontal_minimum(-1408.569);
    ui->set_horizontal_maximum(1408.569);
    ui->set_vertical_minimum(-2029);
    ui->set_vertical_maximum(2029);
    ui->set_image_width(281);
    ui->set_image_height(407);
    test_window->render();
    centre();
    const float upright_width = width;
    slice.axis = 0;
    ui->set_horizontal_minimum(-2029);
    ui->set_horizontal_maximum(2029);
    ui->set_vertical_minimum(-1408.569);
    ui->set_vertical_maximum(1408.569);
    ui->set_image_width(407);
    ui->set_image_height(281);
    const double physical_aspect = (4058. * 407 / 406) / (2817.138 * 281 / 280);
    for (int axis : {0, 1}) {
        slice.axis = axis;
        ui->set_displayed_axis(axis);
        test_window->render();
        centre();
        require(
            width > upright_width * 1.9 && std::abs(width / height - physical_aspect) < .001,
            "Horizontal slices fill fullscreen space with equal millimetre scale on both axes"
        );
    }
    ui->hide();
}
void workspaceSelection() {
    namespace fs = std::filesystem;
    const auto directory =
        fs::temp_directory_path() /
        ("tpc-workspace-ui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path path;
        ~Cleanup() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    } cleanup{directory};
    tpc_slint::application::ApplicationContext context(directory / "settings.json");
    vm::ApplicationViewModel model(context);
    auto ui = MainWindow::create();
    auto& window = ui->window();
    auto* main_window = test_window;
    auto field = FieldWindow::create();
    tpc_slint::ui::ApplicationViewBinding binding(model, ui, field);
    require(context.initialize().has_value(), "Workspace loads its actual settings service");
    const std::vector<std::string> channels{"W1R", "W1F", "W1Z"};
    context.events().connection_state_changed.invoke(true);
    context.events().initialization_data_received.invoke(tpc_slint::models::discoverSensors(channels));
    window.set_size(slint::PhysicalSize({1080, 680}));
    ui->show();
    const auto pixels = main_window->render();
    require(
        ui->get_enabled_sensor_count() == 1 && ui->get_sensor_rows()->row_data(0)->enabled,
        "Binding marks the discovered sensor ON in Workspace"
    );
    // Locate the rendered checkbox, independent of page padding and font metrics.
    std::optional<slint::LogicalPosition> toggle;
    const auto is_blue = [](const auto& p) {
        return p.r < 130 && p.g > 80 && p.b > 180 && p.b > p.r * 1.5;
    };
    for (int y = 200; y < 360 && !toggle; ++y)
        for (int x = 220; x < 270; ++x)
            if (is_blue(pixels[y * main_window->size().width + x])) {
                toggle = slint::LogicalPosition({float(x + 8), float(y + 8)});
                break;
            }
    require(toggle.has_value(), "Workspace has a visible, readable checked control");
    int red = 0;
    for (int y = int(toggle->y + 28); y < int(toggle->y + 84); ++y)
        for (int x = int(toggle->x + 48); x < int(toggle->x + 170); ++x) {
            const auto p = pixels[y * main_window->size().width + x];
            red += p.r > 160 && p.r > p.g * 1.5 && p.r > p.b * 1.5;
        }
    require(red > 10, "Unavailable Workspace rows are rendered in red");
    const auto click = [&](float x, float y) {
        const auto point = slint::LogicalPosition({x, y});
        window.dispatch_pointer_move_event(point);
        window.dispatch_pointer_press_event(point, slint::PointerEventButton::Left);
        window.dispatch_pointer_release_event(point, slint::PointerEventButton::Left);
        main_window->render();
    };
    click(toggle->x, toggle->y);
    require(
        !context.settings().currentSettings().sensors_info[0].enabled && ui->get_enabled_sensor_count() == 0,
        "Clicking the Workspace checkbox changes the reconstruction selection through ViewModel and service"
    );
    click(toggle->x + 34, toggle->y);
    require(
        context.settings().currentSettings().sensors_info[0].enabled && ui->get_enabled_sensor_count() == 1,
        "Clicking ON/OFF text re-enables an available sensor"
    );
    click(toggle->x, toggle->y + 56);
    require(
        !ui->get_sensor_rows()->row_data(1)->enabled && !ui->get_sensor_rows()->row_data(1)->selectable,
        "Clicking an unavailable checkbox leaves the sensor OFF"
    );
    ui->set_calculating(true);
    click(toggle->x, toggle->y);
    require(
        context.settings().currentSettings().sensors_info[0].enabled,
        "Selection is locked while reconstruction is running"
    );
    ui->set_calculating(false);
    auto profile = context.settings().currentSettings();
    profile.sensors_info.resize(1);
    const auto file = directory / "another-profile.json";
    {
        std::ofstream output(file);
        output << nlohmann::json(profile).dump(4);
    }
    window.set_size(slint::PhysicalSize({860, 560}));
    main_window->render();
    window.dispatch_pointer_move_event(slint::LogicalPosition({700, 300}));
    window.dispatch_pointer_scroll_event(slint::LogicalPosition({700, 300}), -500, 0);
    // Fluent scrolling animates; verify the settled image instead of its first frame.
    std::this_thread::sleep_for(std::chrono::milliseconds{250});
    const auto scrolled = main_window->render();
    require(
        !is_blue(scrolled[int(toggle->y) * main_window->size().width + int(toggle->x)]) &&
            ui->get_enabled_sensor_count() == 1,
        "A narrow Workspace scrolls its numeric columns without changing sensor selection"
    );
    ui->set_current_tab(1);
    ui->set_settings_path(slint::SharedString(file.string()));
    main_window->render();
    window.dispatch_pointer_move_event(slint::LogicalPosition({780, 400}));
    window.dispatch_pointer_scroll_event(slint::LogicalPosition({780, 400}), 0, -500);
    std::this_thread::sleep_for(std::chrono::milliseconds{250});
    main_window->render();
    click(690, 450);
    require(
        context.settings().settingsPath() == file && ui->get_sensor_rows()->row_count() == 1 &&
            !ui->get_sensor_rows()->row_data(0)->selectable,
        "Load file button applies a different configuration and requires fresh server discovery"
    );
    drainUiTasks();
    ui->hide();
}
}  // namespace
int main() {
    try {
        fieldMapInteraction();
        workspaceSelection();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Field plot interaction, grid alignment and colour scale checks passed\n";
}
