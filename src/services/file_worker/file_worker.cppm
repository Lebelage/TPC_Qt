module;
#include <expected>
#include <string>
export module tpc_qt.services.file_worker;
import tpc_qt.models.ui.application_settings_model;
export namespace tpc_qt::services {
    struct AppDirectories {
        static const inline std::string SETTINGS_DIR = "Settings";
    };

    struct AppFiles {
        static const inline std::string SETTINGS_JSON_FILE = "settings.json";
    };

    class FileWorker {
    public:
        static FileWorker &instance();

        FileWorker(const FileWorker &) = delete;

        FileWorker &operator=(const FileWorker &) = delete;

        FileWorker(FileWorker &&) = delete;

        FileWorker &operator=(FileWorker &&) = delete;

        ~FileWorker();

    private:
        FileWorker();

    public:
        std::expected<void, std::string> write_settings(std::string settings_text);

        std::expected<std::string, std::string> load_settings();

        bool is_settings_file_exists_or_empty();

    private:
        void initialize();
    };
}