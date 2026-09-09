#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "viewmodel/MainViewModel.hpp"

import tpc_qt.services.settings_holder;
import tpc_qt.services.tpc_srvice;
import tpc_qt.services.event_dispatcher;
import tpc_qt.services.file_worker;

void register_services() {
    tpc_qt::services::TpcService::instance();
    tpc_qt::services::EventDispatcher::instance();
    tpc_qt::services::SettingsHolderService::instance();
    tpc_qt::services::FileWorker::instance();
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
