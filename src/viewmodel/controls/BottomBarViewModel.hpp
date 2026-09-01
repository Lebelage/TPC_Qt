#pragma once
#include <QObject>
#include <QString>
#include <QtQmlIntegration>

#include "client/client.hpp"
#include <models/ui/tabs_indexing_model.hpp>

namespace tpc_qt::view_models {
    class BottomBarViewModel : public QObject {
        Q_OBJECT

        Q_PROPERTY(bool isConnected READ get_isConnected NOTIFY isConnectedChanged)
        Q_PROPERTY(QString endpoint READ get_endpoint WRITE set_endpoint NOTIFY endpointChanged)
        Q_PROPERTY(QString switch_tab_button_name READ get_switch_tab_button_name NOTIFY switchTabButtonNameChanged)

    public:
        explicit BottomBarViewModel(QObject *parent = nullptr, models::ui::TabsIndexingModel target_tab = models::ui::TabsIndexingModel::Settings);

    public:

        bool get_isConnected() const noexcept;

        QString get_endpoint() const;
        void set_endpoint(const QString &endpoint);

        QString get_switch_tab_button_name() const;
        void set_switch_tab_button_name();

    public:
        Q_INVOKABLE void connection_command();

        Q_INVOKABLE void disconnection_command();

        Q_INVOKABLE void switch_tab_command();

    signals:
        void isConnectedChanged();

        void endpointChanged();

        void switchTabButtonNameChanged();

    private:
        auto on_connection_state_changed(tpc::system::client::ConnectionState) -> void;

    private:
        models::ui::TabsIndexingModel current_tab_;

        std::mutex mutex_{};

        bool connection_state_{false};

        bool is_connected_{false};

        QString endpoint_{"opc.tcp://127.0.0.1:1234"};

        QString switch_tab_button_name_{};
    };
}
