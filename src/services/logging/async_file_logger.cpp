#include "services/logging/async_file_logger.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <format>
#include <stdexcept>
#include <utility>

namespace tpc_slint::services {
namespace {
std::string_view boundedUtf8(std::string_view value, std::size_t limit) {
    auto end = std::min(value.size(), limit);
    while (end < value.size() && end > 0 && (static_cast<unsigned char>(value[end]) & 0xc0) == 0x80) --end;
    return value.substr(0, end);
}
std::string escaped(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char c : value) {
        if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else if (c < 32 || c == 127) result += '?';
        else result += static_cast<char>(c);
    }
    return result;
}
}

AsyncFileLogger::AsyncFileLogger(std::filesystem::path file, LogOptions options)
    : file_(std::move(file)), options_(options) {
    if (file_.empty() || options_.maximum_file_bytes < 256 || options_.retained_files > 100
        || options_.queue_capacity == 0 || options_.flush_interval.count() <= 0)
        throw std::invalid_argument("Invalid logging options");
    worker_ = std::jthread([this] { run(); });
}

AsyncFileLogger::~AsyncFileLogger() { stop(); }

void AsyncFileLogger::write(LogLevel level, std::string_view source, std::string_view message) noexcept {
    try {
        Record record{std::chrono::system_clock::now(), level,
            std::string{boundedUtf8(source, 64)}, std::string{boundedUtf8(message, 4096)}};
        if (message.size() > 4096) record.message += " [truncated]";
        {
            std::scoped_lock lock{mutex_};
            if (!accepting_) return;
            if (queue_.size() >= options_.queue_capacity) { ++lost_; return; }
            queue_.push_back(std::move(record));
        }
        ready_.notify_one();
    } catch (...) { ++lost_; }
}

void AsyncFileLogger::stop() noexcept {
    {
        std::scoped_lock lock{mutex_};
        accepting_ = false;
    }
    ready_.notify_one();
    if (worker_.joinable()) worker_.join();
}

std::string AsyncFileLogger::lastError() const {
    std::scoped_lock lock{error_mutex_};
    return error_;
}

void AsyncFileLogger::setError(std::string error) {
    std::scoped_lock lock{error_mutex_};
    if (error_ != error) std::fprintf(stderr, "TPC log: %s\n", error.c_str());
    error_ = std::move(error);
}

bool AsyncFileLogger::openFile() {
    std::error_code error;
    if (!file_.parent_path().empty()) std::filesystem::create_directories(file_.parent_path(), error);
    if (error) { setError("Cannot create log directory: " + error.message()); return false; }
    const bool exists = std::filesystem::exists(file_, error);
    if (error) { setError("Cannot inspect log: " + error.message()); return false; }
    file_bytes_ = exists ? std::filesystem::file_size(file_, error) : 0;
    if (error) { setError("Cannot determine log size: " + error.message()); return false; }
    stream_.clear();
    stream_.open(file_, std::ios::binary | std::ios::app);
    if (!stream_) { setError("Cannot open log: " + file_.string()); return false; }
    {
        std::scoped_lock lock{error_mutex_};
        error_.clear();
    }
    return true;
}

bool AsyncFileLogger::flush() {
    if (!stream_.is_open()) return false;
    stream_.flush();
    if (stream_) return true;
    setError("Cannot flush log: " + file_.string());
    stream_.close();
    return false;
}

bool AsyncFileLogger::rotate() {
    if (!flush()) return false;
    stream_.close();
    const auto archive = [this](std::size_t index) {
        return std::filesystem::path{file_.string() + "." + std::to_string(index)};
    };
    std::error_code error;
    std::filesystem::remove(options_.retained_files ? archive(options_.retained_files) : file_, error);
    if (!error && options_.retained_files) {
        for (std::size_t index = options_.retained_files; index > 1; --index) {
            const auto source = archive(index - 1);
            if (std::filesystem::exists(source, error) && !error)
                std::filesystem::rename(source, archive(index), error);
            if (error) break;
        }
        if (!error) std::filesystem::rename(file_, archive(1), error);
    }
    if (error) { setError("Cannot rotate log: " + error.message()); return false; }
    return openFile();
}

bool AsyncFileLogger::append(std::string line) {
    if (line.size() > options_.maximum_file_bytes) {
        line.resize(boundedUtf8(line, options_.maximum_file_bytes - 15).size());
        line += " [truncated]\n";
    }
    if (file_bytes_ > options_.maximum_file_bytes - line.size() && !rotate()) return false;
    stream_.write(line.data(), static_cast<std::streamsize>(line.size()));
    if (!stream_) {
        setError("Cannot write log: " + file_.string());
        stream_.close();
        return false;
    }
    file_bytes_ += line.size();
    return true;
}

std::string AsyncFileLogger::formatRecord(const Record& record) {
    const auto seconds = std::chrono::floor<std::chrono::seconds>(record.time);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(record.time - seconds).count();
    const auto time = std::chrono::system_clock::to_time_t(seconds);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    char date[32]{}, offset[16]{};
    std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &local);
    std::strftime(offset, sizeof(offset), "%z", &local);
    const auto level = record.level == LogLevel::Error ? "ERROR" : record.level == LogLevel::Warning ? "WARN" : "INFO";
    return std::format("{}.{:03}{} [{}] [{}] {}\n", date, milliseconds, offset, level,
        escaped(record.source), escaped(record.message));
}

void AsyncFileLogger::run() noexcept {
    try {
        using Clock = std::chrono::steady_clock;
        auto next_flush = Clock::now() + options_.flush_interval;
        auto next_open = Clock::now();
        std::deque<Record> batch;
        for (;;) {
            bool stopping;
            {
                std::unique_lock lock{mutex_};
                ready_.wait_until(lock, next_flush, [this] { return !queue_.empty() || !accepting_; });
                batch.swap(queue_);
                stopping = !accepting_;
            }
            if (!stream_.is_open() && Clock::now() >= next_open) {
                (void)openFile();
                next_open = Clock::now() + std::chrono::seconds{1};
            }
            if (stream_.is_open()) {
                const auto lost = lost_.exchange(0);
                if (lost && !append(formatRecord({std::chrono::system_clock::now(), LogLevel::Warning, "logger",
                    std::format("{} records lost due to queue overflow or file I/O failure", lost)})))
                    lost_.fetch_add(lost);
                for (const auto& record : batch) {
                    if (!stream_.is_open() || !append(formatRecord(record))) ++lost_;
                }
            } else lost_.fetch_add(batch.size());
            batch.clear();
            if (Clock::now() >= next_flush || stopping) {
                (void)flush();
                next_flush = Clock::now() + options_.flush_interval;
            }
            if (stopping) {
                if (const auto lost = lost_.load(); lost)
                    std::fprintf(stderr, "TPC log: %zu records could not be persisted\n", lost);
                stream_.close();
                break;
            }
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "TPC log worker failed: %s\n", error.what());
    } catch (...) { std::fprintf(stderr, "TPC log worker failed\n"); }
}

} // namespace tpc_slint::services
