#include "services/logging/logging_service.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>
#include <vector>

namespace s = tpc_slint::services;
namespace fs = std::filesystem;
int checks = 0;
void require(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
struct TemporaryDirectory {
    fs::path path = fs::temp_directory_path() / ("tpc-log-tests-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryDirectory() { if (!fs::create_directory(path)) throw std::runtime_error("Cannot create isolated test directory"); }
    ~TemporaryDirectory() { std::error_code error; fs::remove_all(path, error); }
};
std::vector<std::string> lines(const fs::path& file) {
    std::ifstream input{file};
    std::vector<std::string> result;
    for (std::string line; std::getline(input, line);) result.push_back(std::move(line));
    return result;
}
std::string contents(const fs::path& file) {
    std::ifstream input{file};
    return {std::istreambuf_iterator<char>{input}, {}};
}
void parallel(const fs::path& directory) {
    const auto file = directory / "parallel.log";
    s::LogOptions options;
    options.queue_capacity = 10000;
    const auto start = std::chrono::steady_clock::now();
    {
        s::AsyncFileLogger logger{file, options};
        std::vector<std::jthread> threads;
        for (int thread = 0; thread < 4; ++thread) threads.emplace_back([&, thread] {
            for (int n = 0; n < 1000; ++n)
                logger.write(s::LogLevel::Info, "parallel", std::format("thread={}, record={}",thread,n));
        });
        threads.clear(); // Join all producers before logger destruction.
        const auto elapsed = std::chrono::steady_clock::now() - start;
        std::cout << "4000 records, 4 producers: "
            << std::chrono::duration<double,std::milli>(elapsed).count() << " ms enqueue\n";
        logger.write(s::LogLevel::Warning, "escaping", "line1\nline2\r\ttail");
        logger.write(s::LogLevel::Error, "bounds", std::string(100000,'x'));
    }
    const auto log = lines(file);
    require(log.size() == 4002, "Shutdown drains all queued records");
    require(std::regex_search(log.front(),std::regex{R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{3}[+-][0-9]{4} \[INFO\])"}),
        "Date, milliseconds, timezone, level recorded");
    std::set<std::string> unique;
    for (const auto& line : log) if (const auto pos = line.find("thread="); pos != std::string::npos)
        unique.insert(line.substr(pos));
    require(unique.size() == 4000, "Concurrent messages not corrupted or duplicated");
    require(contents(file).find("line1\\nline2\\r\\ttail") != std::string::npos, "Multiline records escaped");
    require(log.back().size() < 4300 && log.back().find("truncated") != std::string::npos, "Huge messages bounded");
    {
        s::AsyncFileLogger logger{file};
        logger.write(s::LogLevel::Info,"restart","append-marker");
    }
    require(lines(file).size() == 4003, "Restart appends, preserving existing log");
}
void rotation(const fs::path& directory) {
    const auto file = directory / "rotate.log";
    s::LogOptions options;
    options.maximum_file_bytes = 512;
    options.retained_files = 3;
    {
        s::AsyncFileLogger logger{file, options};
        for(int n=0;n<80;++n)logger.write(s::LogLevel::Info,"rotation", std::format("record {} {}",n,std::string(80,'r')));
    }
    require(contents(file).find("record 79") != std::string::npos,"Newest records retained after rotation");
    for (int index = 0; index <= 3; ++index) {
        const auto archive = index ? fs::path{file.string()+"."+std::to_string(index)} : file;
        require(fs::exists(archive) && fs::file_size(archive) <= 512,"Rotated files bounded");
    }
    require(!fs::exists(file.string()+".4"),"Archive retention enforced");
}
void eventLogging(const fs::path& directory) {
    const auto file = directory / "events.log";
    s::EventDispatcher events;
    {
        s::LoggingService logger{events, file};
        tpc_slint::models::ScientificStatus status;
        status.data_message = "stale-channel-test";
        for (int n = 0; n < 1000; ++n) events.scientific_status_changed.invoke(status);
        status.data_message = "recovered-channel-test";
        status.data_ready = true;
        events.scientific_status_changed.invoke(status);
        events.error_occurred.invoke("backend-error-test");
        events.diagnostic.invoke(s::LogLevel::Info,"export","export-test");
        events.field_was_calculated_.invoke(true);
    }
    int stale = 0;
    for (const auto& line : lines(file)) stale += line.find("stale-channel-test") != std::string::npos;
    require(stale == 1,"Unchanged quality warning logged only once");
    const auto text = contents(file);
    require(text.find("recovered-channel-test") != std::string::npos,"Quality recovery logged");
    require(text.find("[ERROR] [backend] backend-error-test") != std::string::npos,"Backend errors logged");
    require(text.find("export-test") != std::string::npos && text.find("Calculation completed") != std::string::npos,"Operations logged");
    require(text.find("Session ended") != std::string::npos,"Session end persisted");
    require(events.diagnostic.empty() && events.scientific_status_changed.empty(),"Subscriptions cleaned up");
}
void fileFailure(const fs::path& directory) {
    const auto not_directory = directory / "blocked";
    {std::ofstream marker{not_directory};marker<<"file, not directory";}
    s::AsyncFileLogger logger{not_directory / "tpc.log"};
    logger.write(s::LogLevel::Error,"failure","must-not-crash");
    logger.stop();
    require(!logger.lastError().empty(),"File failure reported without crashing producers");
}
void overflow(const fs::path& directory) {
    const auto file = directory / "overflow.log";
    s::LogOptions options;
    options.queue_capacity = 1;
    {
        s::AsyncFileLogger logger{file,options};
        for (int n=0;n<20000;++n)logger.write(s::LogLevel::Info,"load", "overflow-record");
    }
    std::size_t persisted=0, lost=0;
    const std::regex loss{R"(\[logger\] ([0-9]+) records lost)"};
    for(const auto& line:lines(file)){
        persisted+=line.find("overflow-record")!=std::string::npos;
        std::smatch match;
        if(std::regex_search(line,match,loss))lost+=std::stoull(match[1].str());
    }
    require(persisted+lost==20000,"Queue overflow accounted for, not silently lost");
}
int main() {
    try {
        TemporaryDirectory directory;
        parallel(directory.path); rotation(directory.path); eventLogging(directory.path); fileFailure(directory.path); overflow(directory.path);
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<checks<<" logging checks passed\n";
}
