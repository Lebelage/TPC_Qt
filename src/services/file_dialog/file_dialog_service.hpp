#pragma once
#include <QtCore>
#include <filesystem>
namespace tpc_qt::services {
class FileDialogService {
public:
    [[nodiscard]]
    static std::optional<std::filesystem::path>
    open_file(
        QWidget* parent = nullptr,
        std::string_view title = "Open file",
        std::string_view filter = "All files (*.*)"
    );

    [[nodiscard]]
    static std::optional<std::filesystem::path>
    save_file(
        QWidget* parent = nullptr,
        std::string_view title = "Save file",
        std::string_view filter = "All files (*.*)"
    );

    [[nodiscard]]
    static std::optional<std::filesystem::path>
    select_directory(
        QWidget* parent = nullptr,
        std::string_view title = "Select directory"
    );
};
}  // namespace tpc_qt::services