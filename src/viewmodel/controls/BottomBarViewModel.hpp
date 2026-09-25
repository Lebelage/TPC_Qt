#pragma once

#include <QObject>
#include <QString>

#include "models/ui/tabs_indexing_model.hpp"
#include "services/scoped_subscription.hpp"

namespace tpc_qt::services {
class EventDispatcher;
class SettingsHolderService;
class TpcService;
}

namespace tpc_qt::view_models {

/** Connection state and navigation commands displayed in the bottom bar. */
class BottomBarViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged)
    Q_PROPERTY(QString switchTabButtonName READ switchTabButtonName NOTIFY switchTabButtonNameChanged)

public:
    explicit BottomBarViewModel(
        models::ui::TabsIndexingModel current_tab,
        services::EventDispatcher& events,
        services::SettingsHolderService& settings,
        services::TpcService& tpc,
        QObject* parent = nullptr
    );

    [[nodiscard]] bool isConnected() const noexcept { return is_connected_; }
    [[nodiscard]] QString switchTabButtonName() const;

    Q_INVOKABLE void connectToTpc();
    Q_INVOKABLE void disconnectFromTpc();
    Q_INVOKABLE void toggleTab();

Q_SIGNALS:
    void isConnectedChanged();
    void switchTabButtonNameChanged();

private:
    void onConnectionStateChanged(bool connected);

    services::EventDispatcher& events_;
    services::SettingsHolderService& settings_;
    services::TpcService& tpc_;
    models::ui::TabsIndexingModel current_tab_;
    bool is_connected_{false};
    services::ScopedSubscription<bool> connection_subscription_;
};

}  // namespace tpc_qt::view_models
