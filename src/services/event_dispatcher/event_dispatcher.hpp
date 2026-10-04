#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "models/application_settings_model.hpp"
#include "models/tpc_data_model.hpp"
#include "tpc/utilities/event_handler.hpp"

namespace tpc_slint::services {
/** In-process event bus for communication between independent UI services. */
class EventDispatcher final {
public:
    EventDispatcher() = default;

    EventDispatcher(const EventDispatcher&) = delete;
    EventDispatcher& operator=(const EventDispatcher&) = delete;
    EventDispatcher(EventDispatcher&&) = delete;
    EventDispatcher& operator=(EventDispatcher&&) = delete;

    ~EventDispatcher();

    void dispose() noexcept;

public:
    tpc::utilities::event_handler<bool> connection_state_changed;
    tpc::utilities::event_handler<std::vector<models::SensorName>> initialization_data_received;
    tpc::utilities::event_handler<const models::AppSettings&> settings_changed;
    tpc::utilities::event_handler<const std::unordered_map<std::string, double>&> frame_received;
    tpc::utilities::event_handler<bool> field_was_calculated_;
};

}  // namespace tpc_slint::services
