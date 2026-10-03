#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <qqml.h>
#include <QStandardPaths>
#include <QVariant>

#include <filesystem>

#include "application/application_context.hpp"
#include "view/FieldSliceItem.hpp"

namespace {
[[nodiscard]] std::filesystem::path toFilesystemPath(const QString& path) {
#ifdef _WIN32
    return std::filesystem::path{path.toStdWString()};
#else
    return std::filesystem::path{path.toStdString()};
#endif
}
}  // namespace

int main(int argc, char *argv[]) {
    QQuickStyle::setStyle("Basic");
    QApplication app(argc, argv);
    qmlRegisterType<tpc_qt::views::FieldSliceItem>("TPC.Native", 1, 0, "FieldSliceItem");
    QCoreApplication::setOrganizationName("TPC");
    QCoreApplication::setApplicationName("TPC_Qt");
    const QDir settings_directory{QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)};
    const auto settings_path = toFilesystemPath(settings_directory.filePath("settings.json"));
    const auto legacy_settings_path = std::filesystem::current_path() / "Settings" / "settings.json";

    tpc_qt::application::ApplicationContext context{settings_path, legacy_settings_path};
    auto& view_model = context.mainViewModel();

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"mainViewModel", QVariant::fromValue(&view_model)}});

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );
    

    engine.loadFromModule("TPC", "Main");

    // Publish initial settings only after every service and view model has
    // subscribed to the application event stream.
    if (const auto initialized = context.initialize(); !initialized) {
        qWarning().noquote() << "Settings were reset to defaults:" << QString::fromStdString(initialized.error());
    }

    return app.exec();
}
