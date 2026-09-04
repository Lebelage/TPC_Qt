#include "settings_holder.hpp"

namespace tpc_qt::services {
#pragma region Constructor/Destructor
    SettingsHolderService &SettingsHolderService::instance() {
        static SettingsHolderService service;
        return service;
    }

    SettingsHolderService::SettingsHolderService() {
        current_settings_ = initialize_by_defaults();
    }

    SettingsHolderService::~SettingsHolderService() {
    }

#pragma endregion


#pragma region Public methods

    void SettingsHolderService::apply_settings(models::AppSettings settings) {
        current_settings_ = settings;
    }

    const models::AppSettings &SettingsHolderService::get_current_settings() const {
        return current_settings_;
    }

#pragma endregion

#pragma region Private methods
    models::AppSettings SettingsHolderService::initialize_by_defaults() {
        return models::AppSettings{{228, 1337}, {"opc.tcp://127.0.0.1:1234", 1000}};
    }
#pragma endregion
}
