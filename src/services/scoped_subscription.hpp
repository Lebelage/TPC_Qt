#pragma once

#include <utility>

#include "tpc/utilities/event_handler.hpp"

namespace tpc_qt::services {

/**
 * Owns one event-handler subscription and removes it on destruction.
 *
 * The referenced event must outlive this object. ApplicationContext guarantees
 * that the event dispatcher is destroyed after its subscribers.
 */
template <class... Args>
class ScopedSubscription final {
public:
    using Event = tpc::utilities::event_handler<Args...>;

    ScopedSubscription() = default;

    ScopedSubscription(const ScopedSubscription&) = delete;
    ScopedSubscription& operator=(const ScopedSubscription&) = delete;
    ScopedSubscription(ScopedSubscription&&) = delete;
    ScopedSubscription& operator=(ScopedSubscription&&) = delete;

    ~ScopedSubscription() {
        reset();
    }

    template <class Handler>
    void subscribe(Event& event, Handler&& handler) {
        reset();
        event_ = &event;
        id_ = event.subscribe(std::forward<Handler>(handler));
    }

    void reset() noexcept {
        if (event_ != nullptr) {
            event_->unsubscribe(id_);
            event_ = nullptr;
            id_ = 0;
        }
    }

private:
    Event* event_{nullptr};
    typename Event::Id id_{0};
};

}  // namespace tpc_qt::services
