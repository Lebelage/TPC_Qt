#include "BottomBarViewModel.hpp"

#include <mutex>

#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/tpc_service/tpc_service.hpp"
#include "services/settings_holder/settings_holder.hpp"

namespace tpc_qt::view_models {
#pragma region Constructor/Destructor
    BottomBarViewModel::BottomBarViewModel(QObject *parent, models::ui::TabsIndexingModel target_tab)
        : QObject(parent), current_tab_(target_tab) {
        tpc_qt::services::TpcService::instance().connection_state_changed_.subscribe(
            [this](tpc::system::client::ConnectionState state) {
                on_connection_state_changed(state);
            });
    }

#pragma endregion

#pragma region Properties

#pragma region [Properties]: isConnected
    bool BottomBarViewModel::get_isConnected() const noexcept {
        return is_connected_;
    }
#pragma endregion

#pragma region [Properties]: switch_tab_button_name
    QString BottomBarViewModel::get_switch_tab_button_name() const {
        return current_tab_ == models::ui::TabsIndexingModel::Settings ? "Workspace" : "Settings";
    }

    void BottomBarViewModel::set_switch_tab_button_name() {
    }
#pragma endregion
#pragma endregion

#pragma region Commands

    void BottomBarViewModel::connection_command() {
        std::scoped_lock (mutex_);

        ///
        services::TpcService::instance().calculate_field_3d();

        services::TpcService::instance().connect_async(
            services::SettingsHolderService::instance().get_current_settings().connection.endpoint);
    }

    void BottomBarViewModel::disconnection_command() {
        std::scoped_lock (mutex_);

        tpc_qt::services::TpcService::instance().disconnect_async();
    }

    void BottomBarViewModel::switch_tab_command() {
        current_tab_ = current_tab_ == models::ui::TabsIndexingModel::Settings
                           ? models::ui::TabsIndexingModel::Workspace
                           : models::ui::TabsIndexingModel::Settings;

        services::EventDispatcher::instance().tab_change_requested.invoke(current_tab_);

        emit switchTabButtonNameChanged();
    }

#pragma endregion

#pragma region Methods
#pragma endregion

#pragma region Handlers
    auto BottomBarViewModel::on_connection_state_changed(tpc::system::client::ConnectionState state) -> void {
        std::scoped_lock (mutex_);

        switch (state) {
            case tpc::system::client::ConnectionState::Disconnected:
                is_connected_ = false;
                break;
            case tpc::system::client::ConnectionState::Connected:
                is_connected_ = true;
                break;
        }
        emit isConnectedChanged();
    }
#pragma endregion
}
