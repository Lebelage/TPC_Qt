#pragma once

#include <filesystem>
#include <optional>

namespace tpc_slint::ui {

/** Opens a native save dialog and returns the selected VTK output path. */
[[nodiscard]] std::optional<std::filesystem::path> showVtkSaveDialog();

}  // namespace tpc_slint::ui
