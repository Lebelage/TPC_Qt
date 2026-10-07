#pragma once

#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/scoped_subscription.hpp"

namespace tpc_slint::services {

/** Records application events without polling or logging every telemetry sample. */
class LoggingService final {
public:
    LoggingService(EventDispatcher& events, std::filesystem::path file, LogOptions options = {});
    ~LoggingService();
    [[nodiscard]] AsyncFileLogger& logger() noexcept { return logger_; }
private:
    AsyncFileLogger logger_;
    std::mutex status_mutex_;
    std::string last_data_, last_field_;
    bool last_data_ready_{}, last_homogeneous_{};
    ScopedSubscription<LogLevel, std::string_view, std::string_view> diagnostic_subscription_;
    ScopedSubscription<std::string> error_subscription_;
    ScopedSubscription<bool> connection_subscription_;
    ScopedSubscription<const models::ScientificStatus&> quality_subscription_;
    ScopedSubscription<const models::AppSettings&> settings_subscription_;
    ScopedSubscription<bool> calculation_subscription_;
};

} // namespace tpc_slint::services
