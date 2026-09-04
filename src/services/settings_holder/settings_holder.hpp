#pragma once
#include <expected>
#include <string>

#include <models/application_settings.h>
using namespace tpc_qt::models;

namespace tpc_qt::services {

    struct AppSettings;

    class SettingsHolderService {
    public:
        static SettingsHolderService &instance();

        SettingsHolderService(const SettingsHolderService &) = delete;

        SettingsHolderService &operator=(const SettingsHolderService &) = delete;

        SettingsHolderService(SettingsHolderService &&) = delete;

        SettingsHolderService &operator=(SettingsHolderService &&) = delete;

        ~SettingsHolderService();

    public:
        void apply_settings(models::AppSettings settngs);

        const models::AppSettings& get_current_settings() const;


    private:
        SettingsHolderService();

    private:
        models::AppSettings initialize_by_defaults();

    private:
        models::AppSettings current_settings_;
    };
}
