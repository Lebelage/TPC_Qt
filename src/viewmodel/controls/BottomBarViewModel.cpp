#include "BottomBarViewModel.hpp"

#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/settings_holder/settings_holder.hpp"
#include "services/tpc_service/tpc_service.hpp"

namespace tpc_qt::view_models {

BottomBarViewModel::BottomBarViewModel(
    models::ui::TabsIndexingModel current_tab,
    services::EventDispatcher& events,
    services::SettingsHolderService& settings,
    services::TpcService& tpc,
    QObject* parent
) : QObject(parent), events_(events), settings_(settings), tpc_(tpc), current_tab_(current_tab) {
    connection_subscription_.subscribe(
        events_.connection_state_changed,
        [this](bool connected) {
            QMetaObject::invokeMethod(this, [this, connected] { onConnectionStateChanged(connected); });
        }
    );
}

QString BottomBarViewModel::switchTabButtonName() const {
    return current_tab_ == models::ui::TabsIndexingModel::Settings ? "Workspace" : "Settings";
}

void BottomBarViewModel::connectToTpc() {
    const auto settings = settings_.currentSettings();
    (void)tpc_.connectAsync(settings.connection.endpoint);
}

void BottomBarViewModel::disconnectFromTpc() {
    tpc_.disconnect();
}

void BottomBarViewModel::toggleTab() {
    current_tab_ = current_tab_ == models::ui::TabsIndexingModel::Settings
        ? models::ui::TabsIndexingModel::Workspace
        : models::ui::TabsIndexingModel::Settings;

    events_.tab_change_requested.invoke(current_tab_);
    Q_EMIT switchTabButtonNameChanged();
}

void BottomBarViewModel::onConnectionStateChanged(bool connected) {
    if (is_connected_ == connected) {
        return;
    }

    is_connected_ = connected;
    Q_EMIT isConnectedChanged();
}

}  // namespace tpc_qt::view_models
