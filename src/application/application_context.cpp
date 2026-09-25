#include "application/application_context.hpp"

#include <utility>

namespace tpc_qt::application {

ApplicationContext::ApplicationContext(
    std::filesystem::path settings_path,
    std::optional<std::filesystem::path> legacy_settings_path
)
    : file_worker_(std::move(settings_path), std::move(legacy_settings_path)),
      settings_(events_, file_worker_),
      tpc_(events_),
      field_slices_(events_, tpc_),
      main_view_model_(events_, settings_, tpc_, field_slices_) {}

ApplicationContext::~ApplicationContext() {
    // Stop worker threads while all event subscribers are still alive.
    field_slices_.dispose();
    tpc_.dispose();
}

std::expected<void, std::string> ApplicationContext::initialize() {
    return settings_.loadSettings();
}

view_models::MainViewModel& ApplicationContext::mainViewModel() noexcept {
    return main_view_model_;
}

}  // namespace tpc_qt::application
