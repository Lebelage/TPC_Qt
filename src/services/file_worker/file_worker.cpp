#include "services/file_worker/file_worker.hpp"

#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
namespace tpc_qt::services {
namespace fs = std::filesystem;

#pragma region Constructor/Destructor
FileWorker& FileWorker::instance() {
    static FileWorker inst;
    return inst;
}

FileWorker::FileWorker() {
    initialize();
}

FileWorker::~FileWorker() {}
#pragma endregion Constructor / Destructor

#pragma region Public methods
std::expected<void, std::string> FileWorker::write_settings(std::string settings_text) {
    auto current_dir = fs::current_path();
    auto settings_dir = current_dir / AppDirectories::SETTINGS_DIR;

    if (!fs::exists(settings_dir))
        return std::unexpected(std::format("{}: directory does not exist", AppDirectories::SETTINGS_DIR));

    fs::path filePath = settings_dir / AppFiles::SETTINGS_JSON_FILE;
    if (!fs::exists(filePath))
        return std::unexpected(std::format("{}: file does not exist", AppFiles::SETTINGS_JSON_FILE));

    std::ofstream file(filePath, std::ios::out);

    if (file.is_open()) {
        file << settings_text;
        file.close();
    } else {
        return std::unexpected(std::format("{}: failed to open file", AppFiles::SETTINGS_JSON_FILE));
    }

    return {};
}

std::expected<std::string, std::string> FileWorker::load_settings() {
    auto current_dir = fs::current_path();
    auto settings_dir = current_dir / AppDirectories::SETTINGS_DIR;

    if (!fs::exists(settings_dir))
        return std::unexpected(std::format("{}: directory does not exist", AppDirectories::SETTINGS_DIR));

    fs::path filePath = settings_dir / AppFiles::SETTINGS_JSON_FILE;

    if (!fs::exists(filePath))
        return std::unexpected(std::format("{}: file does not exist", AppFiles::SETTINGS_JSON_FILE));

    std::ifstream file(filePath);

    std::stringstream buffer;

    if (file.is_open()) {
        buffer << file.rdbuf();
        file.close();
    } else {
        return std::unexpected(std::format("{}: failed to open file", AppFiles::SETTINGS_JSON_FILE));
    }

    if (buffer.str().empty())
        return std::unexpected(std::format("{}: file is empty", AppFiles::SETTINGS_JSON_FILE));

    return buffer.str();
}

bool FileWorker::is_settings_file_exists_or_empty() {
    const fs::path file_path = fs::path(AppDirectories::SETTINGS_DIR) / AppFiles::SETTINGS_JSON_FILE;
    return !fs::exists(file_path) || fs::is_empty(file_path);
}

#pragma endregion Public methods

#pragma region Private methods
void FileWorker::initialize() {
    auto current_dir = fs::current_path();

    auto settings_dir = current_dir / AppDirectories::SETTINGS_DIR;
    if (!fs::exists(settings_dir)) {
        if (fs::create_directories(settings_dir)) {
            ///
        } else {
            ///
        }
    } else {
    }

    fs::path filePath = settings_dir / AppFiles::SETTINGS_JSON_FILE;

    if (!fs::exists(filePath)) {
        std::ofstream outFile(filePath);
        if (outFile.is_open()) {
            outFile.close();
        } else {
            return;
        }
    } else {
    }
}
#pragma endregion Private methods
}  // namespace tpc_qt::services
