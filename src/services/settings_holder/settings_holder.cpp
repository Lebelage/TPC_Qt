module;
#include <iostream>

#include "nlohmann/json.hpp"
module tpc_qt.services.settings_holder;
import tpc_qt.services.file_worker;

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

        nlohmann::json j = settings;
        services::FileWorker::instance().write_settings(j.dump(4));
    }

    std::expected<void, std::string> SettingsHolderService::load_settings() {
        try {
            auto result = services::FileWorker::instance().load_settings();

            // if (!result)
            //      return result.error;

            models::AppSettings deserialized = nlohmann::json::parse(result.value()).get<models::AppSettings>();

            current_settings_ = deserialized;

            return {};
        }
        catch (...) {
            current_settings_ = initialize_by_defaults();
            return {};
        }

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
