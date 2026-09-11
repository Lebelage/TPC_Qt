module;
#include "models/ui/tabs_indexing_model.hpp"

export module tpc_qt.services.event_dispatcher;
import event_handler;
import tpc_qt.models.ui.application_settings_model;
import tpc.system.models.system_data;
export namespace tpc_qt::services {
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
        tpc::utilities::event_handler<tpc::system::models::DiscoveryResult> initialization_data_received;
        tpc::utilities::event_handler<models::AppSettings> settings_loaded;
        tpc::utilities::event_handler<models::AppSettings> settings_applied;
    };
}
