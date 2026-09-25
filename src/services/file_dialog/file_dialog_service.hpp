#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

class QWidget;

namespace tpc_qt::services {

/** Native file-dialog operations used by the workspace. */
class FileDialogService final {
public:
    [[nodiscard]] static std::optional<std::filesystem::path> saveFile(
        QWidget* parent = nullptr,
        std::string_view title = "Save file",
        std::string_view filter = "All files (*.*)"
    );
};

}  // namespace tpc_qt::services
