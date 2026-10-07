#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace tpc_slint::services {

enum class LogLevel { Info, Warning, Error };

struct LogOptions {
    std::size_t maximum_file_bytes{5 * 1024 * 1024};
    std::size_t retained_files{5};
    std::size_t queue_capacity{8192};
    std::chrono::milliseconds flush_interval{250};
};

/** Producers only enqueue bounded records; all formatting and file I/O is on one worker. */
class AsyncFileLogger final {
public:
    explicit AsyncFileLogger(std::filesystem::path file, LogOptions options = {});
    ~AsyncFileLogger();
    AsyncFileLogger(const AsyncFileLogger&) = delete;
    AsyncFileLogger& operator=(const AsyncFileLogger&) = delete;

    void write(LogLevel level, std::string_view source, std::string_view message) noexcept;
    void stop() noexcept; // Stop producers, drain pending records, flush, join.
    [[nodiscard]] std::string lastError() const;
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return file_; }

private:
    struct Record {
        std::chrono::system_clock::time_point time;
        LogLevel level;
        std::string source;
        std::string message;
    };
    void run() noexcept;
    bool openFile();
    bool rotate();
    bool append(std::string line);
    bool flush();
    void setError(std::string error);
    static std::string formatRecord(const Record& record);

    const std::filesystem::path file_;
    const LogOptions options_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Record> queue_;
    bool accepting_{true};
    std::atomic<std::size_t> lost_{0};
    mutable std::mutex error_mutex_;
    std::string error_;
    std::ofstream stream_; // Accessed only by worker.
    std::size_t file_bytes_{};
    std::jthread worker_; // Destroyed before synchronization primitives.
};

} // namespace tpc_slint::services
