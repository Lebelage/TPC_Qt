#pragma once

#include <filesystem>

namespace tpc_slint::application {

[[nodiscard]] std::filesystem::path settingsPath();
[[nodiscard]] std::filesystem::path legacySettingsPath();

}  // namespace tpc_slint::application
