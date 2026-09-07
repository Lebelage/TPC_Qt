module tpc_qt.services.file_worker;
namespace tpc_qt::services::file_worker {
#pragma region Constructor/Destructor
    FileWorker::FileWorker() {
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
#pragma endregion Private methods
}
