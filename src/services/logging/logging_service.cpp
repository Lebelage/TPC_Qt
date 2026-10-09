#include "services/logging/logging_service.hpp"
#include <format>

namespace tpc_slint::services {
LoggingService::LoggingService(EventDispatcher& events, std::filesystem::path file, LogOptions options)
    : logger_(std::move(file), options) {
    diagnostic_subscription_.subscribe(events.diagnostic, [this](LogLevel level, auto source, auto message) {
        logger_.write(level, source, message);
    });
    error_subscription_.subscribe(events.error_occurred, [this](const std::string& error) {
        logger_.write(LogLevel::Error, "backend", error);
    });
    connection_subscription_.subscribe(events.connection_state_changed, [this](bool connected) {
        logger_.write(connected ? LogLevel::Info : LogLevel::Warning, "connection",
            connected ? "OPC UA session activated" : "OPC UA session not active");
    });
    quality_subscription_.subscribe(events.scientific_status_changed, [this](const models::ScientificStatus& status) {
        std::scoped_lock lock{status_mutex_};
        // Repeated frames do not create repeated warnings. Recovery/change is recorded.
        if (last_data_ != status.data_message || last_data_ready_ != status.data_ready) {
            last_data_ = status.data_message;
            last_data_ready_ = status.data_ready;
            const bool verified = status.data_ready && status.data_message.find("in gauss") != std::string::npos;
            logger_.write(verified ? LogLevel::Info : LogLevel::Warning, "data-quality", status.data_message);
        }
        if (last_field_ != status.field_message || last_homogeneous_ != status.homogeneous) {
            last_field_ = status.field_message;
            last_homogeneous_ = status.homogeneous;
            logger_.write(status.homogeneous ? LogLevel::Info : LogLevel::Warning, "field-quality", status.field_message);
        }
    });
    settings_subscription_.subscribe(events.settings_changed, [this](const models::AppSettings& settings) {
        // Deliberately omit endpoint/authentication and actual channel data.
        logger_.write(LogLevel::Info, "settings", std::format(
            "Settings applied: {} sensors; radius={} mm; length={} mm; grid={}x{}x{}; reference={}",
            settings.sensors_info.size(), settings.geometry.radius, settings.geometry.length,
            settings.grid[0], settings.grid[1], settings.grid[2], !settings.analysis.reference_map_path.empty()));
    });
    calculation_subscription_.subscribe(events.field_was_calculated_, [this](bool success) {
        logger_.write(success ? LogLevel::Info : LogLevel::Error, "calculation",
            success ? "Calculation completed; slices and VTK available (see quality diagnostics)" : "Calculation failed");
    });
    logger_.write(LogLevel::Info, "application", "Session started");
}

LoggingService::~LoggingService() {
    logger_.write(LogLevel::Info, "application", "Session ended; draining log queue");
    // Subscriptions are destroyed first, then logger drains and joins its worker.
}
} // namespace tpc_slint::services
