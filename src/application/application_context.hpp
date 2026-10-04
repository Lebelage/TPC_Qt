#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

#include "services/event_dispatcher/event_dispatcher.hpp"
#include "services/field_slice/field_slice_service.hpp"
#include "services/file_worker/file_worker.hpp"
#include "services/settings_holder/settings_holder.hpp"
#include "services/tpc_service/tpc_service.hpp"

namespace tpc_slint::application {

/**
 * Application composition root.
 *
 * Owns long-lived services and view models in dependency order. Members are
 * intentionally declared so subscribers are destroyed before the event bus.
 */
class ApplicationContext final {
public:
    ApplicationContext(
        std::filesystem::path settings_path,
        std::optional<std::filesystem::path> legacy_settings_path = std::nullopt
    );

    ApplicationContext(const ApplicationContext&) = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;
    ApplicationContext(ApplicationContext&&) = delete;
    ApplicationContext& operator=(ApplicationContext&&) = delete;

    ~ApplicationContext();

    [[nodiscard]] std::expected<void, std::string> initialize();
    [[nodiscard]] services::EventDispatcher& events() noexcept { return events_; }
    [[nodiscard]] services::SettingsHolderService& settings() noexcept { return settings_; }
    [[nodiscard]] services::TpcService& tpc() noexcept { return tpc_; }
    [[nodiscard]] services::FieldSliceService& fieldSlices() noexcept { return field_slices_; }

private:
    services::EventDispatcher events_;
    services::FileWorker file_worker_;
    services::SettingsHolderService settings_;
    services::TpcService tpc_;
    services::FieldSliceService field_slices_;
};

}  // namespace tpc_slint::application
