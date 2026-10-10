#pragma once

#include <atomic>
#include <cstdint>
#include <exec/start_detached.hpp>
#include <expected>
#include <format>
#include <memory>
#include <mutex>
#include <open62541pp/client.hpp>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <stdexcept>
#include <stdexec/__detail/__task.hpp>
#include <stdexec/execution.hpp>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "tpc/core/definitions/client_definitions.hpp"
#include "tpc/system/client/frame_receiver.hpp"
#include "tpc/system/client/helpers/opcua_browse_adapter.hpp"
#include "tpc/system/client/subscription.hpp"
#include "tpc/system/models/data.hpp"
#include "tpc/utilities/event_handler.hpp"
namespace tpc::system::client {

enum class ConnectionState {
    SessionActivated,
    Connected,
    Inactive,
    SessionClosed,
    Disconnected,
};

class Client {
public:
    [[nodiscard]] static std::expected<std::unique_ptr<Client>, std::string> create(std::string endpoint);

public:
    ~Client() = default;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) = delete;
    Client& operator=(Client&&) = delete;

private:
    Client(std::string endpoint);

public:
    std::vector<std::string> get_sensors_names() const;

public:
    std::expected<bool, std::string> connect_async();

    void stop();

    bool is_running() const;

    auto get_frame() -> std::optional<std::unordered_map<std::string, double>>;
    auto get_telemetry() const -> models::TelemetryFrame;

private:
    [[nodiscard]] auto initialize_opcua_handlers() -> std::expected<void, std::string>;

    [[nodiscard]] auto initialize_monitored_items() -> std::expected<void, std::string>;

    [[nodiscard]] auto create_subscription(models::DiscoveryResult discovery) -> stdexec::task<void>;

    [[nodiscard]] auto discover_folders() -> stdexec::task<models::DiscoveryResult>;

    [[nodiscard]] auto find_child(
        const opcua::BrowseResult& result, std::string_view name, opcua::NodeClass expected_class
    ) -> std::optional<opcua::NodeId>;

    [[nodiscard]] auto find_slot_ids(const opcua::BrowseResult& result) -> std::vector<opcua::NodeId>;

    auto append_channels(models::DiscoveryResult& result, const opcua::BrowseResult& slot_result) -> void;

private:
    auto on_subscription_data_received(opcua::NodeId, opcua::DataValue) -> void;
    auto on_subscription_error_occurred(std::string) -> void;
    auto on_subscription_info_occurred(std::string) -> void;

    auto on_opcua_session_activated() -> void;
    auto on_opcua_connected() -> void;
    auto on_opcua_inactive() -> void;
    auto on_opcua_session_closed() -> void;
    auto on_opcua_disconnected() -> void;

public:
    utilities::event_handler<std::string> error_occurred_;
    utilities::event_handler<std::string> warning_occurred_;
    utilities::event_handler<std::string> info_occurred_;
    utilities::event_handler<ConnectionState> connection_state_changed_;
    utilities::event_handler<models::DiscoveryResult> initialization_data_received_;

private:
    std::unique_ptr<client::Subscription> subscription_;
    std::unique_ptr<opcua::Client> client_;
    std::string endpoint_;

    std::unique_ptr<FrameReceiver> frame_receiver_;

    std::atomic_bool running_{false};
    std::atomic_bool stop_requested_{false};

    std::jthread opcua_thread_;

    models::DiscoveryResult channels_info_;
    mutable std::shared_mutex channels_info_mutex_;

private:
    static constexpr std::uint16_t kPollingIntervalMs = 50;
};
}  // namespace tpc::system::client

namespace tpc::system::client {

inline auto Client::initialize_monitored_items() -> std::expected<void, std::string> {
    frame_receiver_->clear();
    auto task = stdexec::starts_on(stdexec::inline_scheduler{}, discover_folders()) |
                stdexec::let_value([this](models::DiscoveryResult discovery) {
                    {
                        std::unique_lock lock{channels_info_mutex_};
                        channels_info_ = discovery;
                    }

                    initialization_data_received_.invoke(discovery);
                    return create_subscription(std::move(discovery));
                }) |
                stdexec::upon_error([this](std::exception_ptr error) noexcept {
                    try {
                        std::rethrow_exception(error);
                    } catch (const std::exception& ex) {
                        error_occurred_.invoke(std::format("[{}]: {}", core::definitions::CLIENT_ERROR, ex.what()));
                    }
                });

    exec::start_detached(std::move(task));
    return {};
}

inline auto Client::create_subscription(models::DiscoveryResult discovery) -> stdexec::task<void> {
    if (!subscription_)
        throw std::runtime_error("Subscription is null");

    std::vector<opcua::NodeId> node_ids;
    node_ids.reserve(discovery.nodes.size());

    for (const auto& value : discovery.nodes | std::views::keys) {
        node_ids.push_back(value);
    }

    auto result = subscription_->create_subscription(*client_, node_ids);

    if (!result)
        throw std::runtime_error{result.error()};

    co_return;
}

inline auto Client::discover_folders() -> stdexec::task<models::DiscoveryResult> {
    models::DiscoveryResult result;

    auto root =
        co_await tpc::system::client::helpers::browse_async(*client_, opcua::NodeId{opcua::ObjectId::ObjectsFolder});

    auto target_channel_id = find_child(root, "ADC Channels", opcua::NodeClass::Object);
    if (!target_channel_id)
        throw std::runtime_error{"Cannot find target channel ID"};

    auto slots_result = co_await tpc::system::client::helpers::browse_async(*client_, std::move(*target_channel_id));
    auto slot_ids = find_slot_ids(slots_result);

    if (slot_ids.empty())
        throw std::runtime_error{"No slots found"};

    for (auto& slot_id : slot_ids) {
        auto slot_result = co_await tpc::system::client::helpers::browse_async(*client_, std::move(slot_id));
        append_channels(result, slot_result);
    }

    co_return result;
}
}  // namespace tpc::system::client
