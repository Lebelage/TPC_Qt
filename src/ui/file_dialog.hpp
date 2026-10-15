#pragma once

#include <filesystem>
#include <optional>

namespace tpc_slint::ui {

/** Opens a native save dialog and returns the selected VTK output path. */
[[nodiscard]] std::optional<std::filesystem::path> showVtkSaveDialog();
[[nodiscard]] std::optional<std::filesystem::path> showSettingsOpenDialog();

}  // namespace tpc_slint::ui
