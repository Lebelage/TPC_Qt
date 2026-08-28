#include "MainViewModel.hpp"
#include <services/tpc_service/tpc_service.hpp>
MainViewModel::MainViewModel(QObject* parent)
    : QObject(parent)
{
    tpc_qt::services::TpcService::instance();
    tpc_qt::services::TpcService::instance().connection_state_changed_.subscribe([this](tpc::system::client::ConnectionState state) { on_connection_state_changed(state); });
}

bool MainViewModel::connected() const noexcept
{
    return connected_;
}

QString MainViewModel::endpoint() const
{
    return endpoint_;
}

void MainViewModel::setEndpoint(const QString& endpoint)
{
    if (endpoint_ == endpoint)
        return;

    endpoint_ = endpoint;

    emit endpointChanged();
}

void MainViewModel::connectToServer()
{
    if (connected_)
        return;

    tpc_qt::services::TpcService::instance().connect_async(endpoint_.toStdString());
}

void MainViewModel::disconnectFromServer()
{
    tpc_qt::services::TpcService::instance().disconnect_async();
}

#pragma region Handlers
void MainViewModel::on_connection_state_changed(tpc::system::client::ConnectionState state) {
    switch (state) {
        case tpc::system::client::ConnectionState::Disconnected:
            connected_ = false;
            break;
            case tpc::system::client::ConnectionState::Connected:
            connected_ = true;
            break;
    }

    emit connectedChanged();
}
#pragma endregion