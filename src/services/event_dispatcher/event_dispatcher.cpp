#include "services/event_dispatcher/event_dispatcher.hpp"
namespace tpc_slint::services {
EventDispatcher::~EventDispatcher() {
    dispose();
}

void EventDispatcher::dispose() noexcept {
    connection_state_changed.dispose();
    initialization_data_received.dispose();
    settings_changed.dispose();
    frame_received.dispose();
    field_was_calculated_.dispose();
}

}  // namespace tpc_slint::services
