#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "open62541pp/services/detail/client_service.hpp"
#include "open62541pp/services/monitoreditem.hpp"
#include "open62541pp/services/subscription.hpp"
#include "tpc/utilities/event_handler.hpp"
namespace tpc::system::client {

struct DefaultSubscriptionConfig;

class Subscription {
public:
    [[nodiscard]] static std::expected<std::unique_ptr<Subscription>, std::string> create();

    ~Subscription();

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
    Subscription(Subscription&&) = delete;
    Subscription& operator=(Subscription&&) = delete;

private:
    Subscription(opcua::services::SubscriptionParameters parameters);

public:
    std::expected<void, std::string> create_subscription(
        opcua::Client& client, std::span<const opcua::NodeId> channels_id
    );

    std::expected<void, std::string> create_monitored_items(
        opcua::Client& client, opcua::CreateSubscriptionResponse& response
    );

private:
    opcua::services::SubscriptionParameters parameters_;

    std::optional<opcua::IntegerId> subscription_id_;
    std::vector<opcua::IntegerId> monitored_item_ids_;

    std::vector<opcua::NodeId> node_ids_;

private:
    void release(opcua::IntegerId subscription_id) noexcept;

public:
    utilities::event_handler<std::string> info_occurred_;
    utilities::event_handler<std::string> error_occurred_;
    utilities::event_handler<opcua::NodeId, opcua::DataValue> data_received_;
};
}  // namespace tpc::system::client
