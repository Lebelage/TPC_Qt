#include "app.h"

#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "application/application_context.hpp"
#include "application/platform_paths.hpp"
#include "ui/application_view_binding.hpp"
#include "viewmodels/application_view_model.hpp"

namespace {
int runApplication() {
    tpc_slint::application::ApplicationContext context{tpc_slint::application::settingsPath()};

    auto main_window = MainWindow::create();
    auto field_window = FieldWindow::create();
    tpc_slint::viewmodels::ApplicationViewModel view_model{context};
    tpc_slint::ui::ApplicationViewBinding view_binding{view_model, main_window, field_window};

    if (const auto initialized = context.initialize(); !initialized) {
        std::cerr << "Settings initialization failed: " << initialized.error() << '\n';
        view_model.showStartupError(initialized.error());
    }

    main_window->run();
    field_window->hide();
    return 0;
}
}

#ifdef _WIN32
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    return runApplication();
}
#else
int main() {
    return runApplication();
}
#endif
