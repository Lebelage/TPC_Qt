#include "application/platform_paths.hpp"

#include <cstdlib>

namespace tpc_slint::application {
namespace fs = std::filesystem;

fs::path settingsPath() {
#ifdef _WIN32
    if (const char* app_data = std::getenv("APPDATA")) {
        return fs::path{app_data} / "TPC" / "TPC_Slint" / "settings.json";
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        return fs::path{home} / "Library" / "Application Support" / "TPC" / "TPC_Slint" / "settings.json";
    }
#else
    if (const char* xdg_config = std::getenv("XDG_CONFIG_HOME")) {
        return fs::path{xdg_config} / "TPC_Slint" / "settings.json";
    }
    if (const char* home = std::getenv("HOME")) {
        return fs::path{home} / ".config" / "TPC_Slint" / "settings.json";
    }
#endif
    return legacySettingsPath();
}

fs::path legacySettingsPath() {
    return fs::current_path() / "Settings" / "settings.json";
}

}  // namespace tpc_slint::application
