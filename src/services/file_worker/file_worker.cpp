module;
#include <filesystem>
#include <fstream>
#include <expected>
module tpc_qt.services.file_worker;
namespace tpc_qt::services {
    namespace fs = std::filesystem;

#pragma region Constructor/Destructor
    FileWorker &FileWorker::instance() {
        static FileWorker inst;
        return inst;
    }

    FileWorker::FileWorker() {
        initialize();
    }


    FileWorker::~FileWorker() {
    }
#pragma endregion Constructor/Destructor

#pragma region Public methods
    std::expected<void, std::string> FileWorker::write_settings(std::string settings_text) {
        if (!fs::exists(AppDirectories::SETTINGS_DIR))
            return std::unexpected(std::format("{}: directory does not exist", AppDirectories::SETTINGS_DIR));

        fs::path filePath = fs::path(AppDirectories::SETTINGS_DIR) / AppFiles::SETTINGS_JSON_FILE;
        if (!fs::exists(filePath))
            return std::unexpected(std::format("{}: file does not exist", AppFiles::SETTINGS_JSON_FILE));

        std::ofstream file(filePath, std::ios::out);

        if (file.is_open()) {
            file << settings_text;
            file.close();
        }
        else {
            return std::unexpected(std::format("{}: failed to open file", AppFiles::SETTINGS_JSON_FILE));
        }

        return{};
    }

    std::expected<std::string, std::string> FileWorker::load_settings() {

        if (!fs::exists(AppDirectories::SETTINGS_DIR))
            return std::unexpected(std::format("{}: directory does not exist", AppDirectories::SETTINGS_DIR));

        fs::path filePath = fs::path(AppDirectories::SETTINGS_DIR) / AppFiles::SETTINGS_JSON_FILE;
        if (!fs::exists(filePath))
            return std::unexpected(std::format("{}: file does not exist", AppFiles::SETTINGS_JSON_FILE));

        std::ifstream file(filePath);

        std::stringstream buffer;

        if (file.is_open()) {
            buffer << file.rdbuf();
            file.close();
        }
        else {
            return std::unexpected(std::format("{}: failed to open file", AppFiles::SETTINGS_JSON_FILE));
        }

        return buffer.str();
    }

    bool FileWorker::is_settings_file_exists_or_empty() {
    }

#pragma endregion Public methods

#pragma region Private methods
    void FileWorker::initialize() {
        if (!fs::exists(AppDirectories::SETTINGS_DIR)) {
            if (fs::create_directories(AppDirectories::SETTINGS_DIR)) {
                ///
            } else {
                ///
            }
        } else {
        }

        fs::path filePath = fs::path(AppDirectories::SETTINGS_DIR) / AppFiles::SETTINGS_JSON_FILE;

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
}
