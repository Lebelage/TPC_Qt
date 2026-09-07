module tpc_qt.services.event_dispatcher;
namespace tpc_qt::services {

#pragma region Constructor/Destructor
    EventDispatcher &EventDispatcher::instance() {
        static EventDispatcher dispatcher;
        return dispatcher;
    }

    EventDispatcher::EventDispatcher() {}

    EventDispatcher::~EventDispatcher() {
        dispose();
    }

#pragma endregion

    auto EventDispatcher::dispose() -> void {
        tab_change_requested.dispose();
    }
}
