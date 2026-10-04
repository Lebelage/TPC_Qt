#pragma once

#include <expected>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>

namespace tpc_slint::services {

/** Persists the settings JSON at an explicitly configured application path. */
class FileWorker final {
public:
    explicit FileWorker(
        std::filesystem::path settings_path,
        std::optional<std::filesystem::path> legacy_settings_path = std::nullopt
    );

    FileWorker(const FileWorker&) = delete;
    FileWorker& operator=(const FileWorker&) = delete;
    FileWorker(FileWorker&&) = delete;
    FileWorker& operator=(FileWorker&&) = delete;

    [[nodiscard]] std::expected<void, std::string> writeSettings(const std::string& settings_text);
    [[nodiscard]] std::expected<std::string, std::string> loadSettings() const;

private:
    void initialize(const std::optional<std::filesystem::path>& legacy_settings_path);

    std::filesystem::path settings_path_;
    mutable std::mutex mutex_;
};

}  // namespace tpc_slint::services
