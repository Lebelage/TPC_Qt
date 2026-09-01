#pragma once
#include <QObject>

namespace tpc_qt::view_models {
    class SettingsViewModel : public QObject {
        Q_OBJECT

    public:
        explicit SettingsViewModel(QObject *parent = nullptr);
    };
}
