module;
#include <filesystem>
#include <fstream>
module tpc_qt.services.file_worker;
namespace tpc_qt::services::file_worker {
    namespace fs = std::filesystem;

#pragma region Constructor/Destructor
    FileWorker & FileWorker::instance() {
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
    }

    std::expected<models::AppSettings, std::string> FileWorker::load_settings() {
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

        fs::path filePath = fs::path(AppDirectories::SETTINGS_DIR) / AppDirectories::SETTINGS_DIR;

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
