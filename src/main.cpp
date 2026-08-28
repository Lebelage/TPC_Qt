#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "viewmodel/MainViewModel.hpp"

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    MainViewModel view_model;

    engine.rootContext()->setContextProperty(
        "mainViewModel",
        &view_model
    );

    engine.loadFromModule("TPC", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}