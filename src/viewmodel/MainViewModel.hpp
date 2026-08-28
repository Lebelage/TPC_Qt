#pragma once
#include <QObject>

#include "client/client.hpp"

class MainViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(
        bool connected
        READ connected
        NOTIFY connectedChanged
    )

    Q_PROPERTY(
        QString endpoint
        READ endpoint
        WRITE setEndpoint
        NOTIFY endpointChanged
    )

public:
    explicit MainViewModel(QObject *parent = nullptr);

    [[nodiscard]]
    bool connected() const noexcept;

    [[nodiscard]]
    QString endpoint() const;

    void setEndpoint(const QString &endpoint);

    Q_INVOKABLE void connectToServer();

    Q_INVOKABLE void disconnectFromServer();

signals:
    void connectedChanged();

    void endpointChanged();

private:
    void on_connection_state_changed(tpc::system::client::ConnectionState state);
private:
    bool connected_{false};

    QString endpoint_{
        "opc.tcp://127.0.0.1:1234"
    };
};
