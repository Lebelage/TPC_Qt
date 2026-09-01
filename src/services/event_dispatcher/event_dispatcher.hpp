#pragma once
#include "event_handler.hpp"
#include "models/ui/tabs_indexing_model.hpp"

namespace tpc_qt::services {
    class EventDispatcher {
    public:
        static EventDispatcher &instance();

        EventDispatcher(const EventDispatcher &) = delete;

        EventDispatcher &operator=(const EventDispatcher &) = delete;

        EventDispatcher(EventDispatcher &&) = delete;

        EventDispatcher &operator=(EventDispatcher &&) = delete;

        ~EventDispatcher();

    public:
        auto dispose() -> void;

    private:
        EventDispatcher();

    public:
        tpc::utilities::event_handler<models::ui::TabsIndexingModel> tab_change_requested;
    };
}
