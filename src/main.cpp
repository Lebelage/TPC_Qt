#include "app.h"

#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "application/application_context.hpp"
#include "application/platform_paths.hpp"
#include "ui/application_presenter.hpp"

namespace {
int runApplication() {
    tpc_slint::application::ApplicationContext context{
        tpc_slint::application::settingsPath(),
        tpc_slint::application::legacySettingsPath()
    };

    auto main_window = MainWindow::create();
    auto field_window = FieldWindow::create();
    tpc_slint::ui::ApplicationPresenter presenter{context, main_window, field_window};

    if (const auto initialized = context.initialize(); !initialized) {
        std::cerr << "Settings were reset to defaults: " << initialized.error() << '\n';
        presenter.showStartupError(initialized.error());
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
