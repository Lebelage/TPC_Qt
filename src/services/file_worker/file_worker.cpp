#include "services/file_worker/file_worker.hpp"

#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace tpc_slint::services {
namespace fs = std::filesystem;

FileWorker::FileWorker(fs::path settings_path, std::optional<fs::path> legacy_settings_path)
    : settings_path_(std::move(settings_path)) {
    initialize(legacy_settings_path);
}

std::expected<void, std::string> FileWorker::writeSettings(const std::string& settings_text) {
    std::scoped_lock lock{mutex_};
    const fs::path& path = settings_path_;

    std::ofstream file{path, std::ios::trunc};
    if (!file) {
        return std::unexpected{std::format("Failed to open {} for writing", path.string())};
    }

    file << settings_text;
    if (!file) {
        return std::unexpected{std::format("Failed to write {}", path.string())};
    }

    return {};
}

std::expected<std::string, std::string> FileWorker::loadSettings() const {
    std::scoped_lock lock{mutex_};
    const fs::path& path = settings_path_;

    std::ifstream file{path};
    if (!file) {
        return std::unexpected{std::format("Failed to open {} for reading", path.string())};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (buffer.str().empty()) {
        return std::unexpected{std::format("{} is empty", path.string())};
    }

    return buffer.str();
}

void FileWorker::initialize(const std::optional<fs::path>& legacy_settings_path) {
    std::scoped_lock lock{mutex_};
    std::error_code error;
    fs::create_directories(settings_path_.parent_path(), error);

    if (!fs::exists(settings_path_) && legacy_settings_path && fs::exists(*legacy_settings_path)) {
        error.clear();
        fs::copy_file(*legacy_settings_path, settings_path_, fs::copy_options::skip_existing, error);
    }

    if (!fs::exists(settings_path_)) {
        std::ofstream{settings_path_};
    }
}

}  // namespace tpc_slint::services
