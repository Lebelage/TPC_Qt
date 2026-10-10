#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>
namespace tpc::utilities {

template <class... Args>
class event_handler {
public:
    using Handler = std::function<void(Args...)>;
    using Id = std::uint64_t;

    [[nodiscard]]
    Id subscribe(Handler handler) {
        std::lock_guard lock{mutex_};
        const Id id = next_id_++;

        handlers_.emplace(id, std::move(handler));

        return id;
    }

    void unsubscribe(Id id) {
        std::lock_guard lock{mutex_};
        handlers_.erase(id);
    }

    void release() noexcept {
        std::lock_guard lock{mutex_};
        handlers_.clear();
    }

    void dispose() noexcept {
        std::lock_guard lock{mutex_};
        handlers_.clear();
        handlers_.rehash(0);
        next_id_ = 1;
    }

    void invoke(Args... args) const {
        std::vector<Handler> handlers;

        {
            std::lock_guard lock{mutex_};
            handlers.reserve(handlers_.size());

            for (const auto& entry : handlers_)
                handlers.push_back(entry.second);
        }

        for (const auto& handler : handlers) {
            handler(args...);
        }
    }

    void operator()(Args... args) const {
        invoke(std::forward<Args>(args)...);
    }

    [[nodiscard]]
    bool empty() const noexcept {
        std::lock_guard lock{mutex_};
        return handlers_.empty();
    }

    [[nodiscard]]
    std::size_t size() const noexcept {
        std::lock_guard lock{mutex_};
        return handlers_.size();
    }

private:
    Id next_id_{1};
    std::unordered_map<Id, Handler> handlers_;
    mutable std::mutex mutex_;
};

}  // namespace tpc::utilities
