#include "application/application_context.hpp"

#include <utility>

namespace tpc_slint::application {

ApplicationContext::ApplicationContext(std::filesystem::path settings_path)
    : logging_(events_, settings_path.parent_path() / "logs" / "tpc.log"),
      file_worker_(std::move(settings_path)),
      settings_(events_, file_worker_),
      tpc_(events_),
      field_slices_(events_, tpc_) {}

ApplicationContext::~ApplicationContext() {
    // Stop worker threads while all event subscribers are still alive.
    field_slices_.dispose();
    tpc_.dispose();
}

std::expected<void, std::string> ApplicationContext::initialize() {
    return settings_.loadSettings();
}

}  // namespace tpc_slint::application
