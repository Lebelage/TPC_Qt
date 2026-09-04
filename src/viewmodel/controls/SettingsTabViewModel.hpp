#pragma once
#include <QObject>

namespace tpc_qt::view_models {
    class SettingsViewModel : public QObject {
        Q_OBJECT

        Q_PROPERTY(double tpcLength READ get_tpcLength WRITE set_tpcLength NOTIFY tpcLengthChanged)
        Q_PROPERTY(double tpcRadius READ get_tpcRadius WRITE set_tpcRadius NOTIFY tpcRadiusChanged)
        Q_PROPERTY(QString endpoint READ get_endpoint WRITE set_endpoint NOTIFY endpointChanged)
        Q_PROPERTY(int pollingInterval READ get_pollingInterval WRITE set_pollingInterval NOTIFY pollingIntervalChanged)

    public:
        explicit SettingsViewModel(QObject *parent = nullptr);

    public:
        double get_tpcLength() const;

        void set_tpcLength(double tpcLength);

        double get_tpcRadius() const;

        void set_tpcRadius(double tpcRadius);

        QString get_endpoint() const;

        void set_endpoint(const QString &endpoint);

        int get_pollingInterval() const;

        void set_pollingInterval(int pollingInterval);

    public:
        Q_INVOKABLE void apply_settings_command();

    signals:
        void tpcLengthChanged();

        void tpcRadiusChanged();

        void endpointChanged();

        void pollingIntervalChanged();

    private:
        QString endpoint_{""};
        int polling_interval_{0};
        double tpc_length_{0};
        double tpc_radius_{0};
    };
}
