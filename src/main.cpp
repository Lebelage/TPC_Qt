#include "app.h"

#include <iostream>

#include "application/application_context.hpp"
#include "application/platform_paths.hpp"
#include "ui/application_presenter.hpp"

int main() {
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
