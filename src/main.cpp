#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "services/settings_holder/settings_holder.hpp"
#include "services/tpc_service/tpc_service.hpp"
#include "viewmodel/MainViewModel.hpp"

void register_services() {
    tpc_qt::services::TpcService::instance();
    tpc_qt::services::EventDispatcher::instance();
    tpc_qt::services::SettingsHolderService::instance();
}

void unregister_services() {
    tpc_qt::services::TpcService::instance().dispose();
}

int main(int argc, char *argv[]) {
    QQuickStyle::setStyle("Basic");
    QGuiApplication app(argc, argv);

    [[maybe_unused]] auto &service = tpc_qt::services::TpcService::instance();

    tpc_qt::view_models::MainViewModel view_model;

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("mainViewModel", &view_model);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection
    );

    engine.loadFromModule("TPC", "Main");

    register_services();

    app.exec();

    unregister_services();

    return 0;
}
