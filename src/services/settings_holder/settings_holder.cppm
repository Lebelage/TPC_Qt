module;
#include <expected>
#include <string>
#include <vector>

export module tpc_qt.services.settings_holder;
import tpc_qt.models.ui.application_settings_model;
using namespace tpc_qt::models;

export namespace tpc_qt::services {
    struct AppSettings;

    class SettingsHolderService {
    public:
        static SettingsHolderService &instance();

        SettingsHolderService(const SettingsHolderService &) = delete;

        SettingsHolderService &operator=(const SettingsHolderService &) = delete;

        SettingsHolderService(SettingsHolderService &&) = delete;

        SettingsHolderService &operator=(SettingsHolderService &&) = delete;

        ~SettingsHolderService();

    private:
        SettingsHolderService();

    public:
        void apply_settings();

        std::expected<void, std::string> load_settings();

        void set_connection_parameters(ConnectionParameters);
        void set_geometry_parameters(TpcGeometryParams);
        void set_sensors_parameters(std::vector<SensorInfo>);

        // const models::AppSettings &get_current_settings() const;
    
    private:
        models::AppSettings initialize_by_defaults();

    private:
        models::AppSettings current_settings_{};
    };
}
