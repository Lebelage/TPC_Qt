#include "open62541pp/services/subscription.hpp"

#include <expected>
#include <format>
#include <open62541pp/client.hpp>
#include <span>
#include <utility>

#include "open62541pp/services/monitoreditem.hpp"
#include "tpc/system/client/subscription.hpp"
namespace tpc::system::client {

struct DefaultSubscriptionConfig {
    constexpr static double publishing_interval = 1000.;
    constexpr static uint32_t max_keep_alive_count = 10;
};

#pragma region Constructor/Fabric

std::expected<std::unique_ptr<Subscription>, std::string> Subscription::create() {
    try {
        opcua::services::SubscriptionParameters parameters{
            .publishingInterval = DefaultSubscriptionConfig::publishing_interval,
            .maxKeepAliveCount = DefaultSubscriptionConfig::max_keep_alive_count
        };

        return std::unique_ptr<Subscription>{new Subscription(std::move(parameters))};
    } catch (...) {
        return std::unexpected("Failed to create subscription: unknown error");
    }
}

Subscription::Subscription(opcua::services::SubscriptionParameters parameters) : parameters_(std::move(parameters)) {}

Subscription::~Subscription() = default;
#pragma endregion

#pragma region Public methods

std::expected<void, std::string> Subscription::create_subscription(
    opcua::Client& client, std::span<const opcua::NodeId> channels_id
) {
    node_ids_ = std::vector<opcua::NodeId>{channels_id.begin(), channels_id.end()};

    opcua::services::createSubscriptionAsync(
        client,
        parameters_,
        true,
        nullptr,
        [this](opcua::IntegerId subscription_id) {
            release(subscription_id);
        },
        [this, &client](opcua::CreateSubscriptionResponse& response) noexcept {
            create_monitored_items(client, response);
        }
    );

    return {};
}

std::expected<void, std::string> Subscription::create_monitored_items(
    opcua::Client& client, opcua::CreateSubscriptionResponse& response
) {
    try {
        opcua::services::MonitoringParametersEx parameters{
            .samplingInterval = 1000,
            .filter = opcua::ExtensionObject{opcua::DataChangeFilter{
                opcua::DataChangeTrigger::StatusValueTimestamp, opcua::DeadbandType::None, 0.0}},
            .queueSize = 1, .discardOldest = true
        };

        response.responseHeader().serviceResult().throwIfBad();

        subscription_id_ = response.subscriptionId();
        for (const auto& node : node_ids_) {
            auto read_value = opcua::ReadValueId{node, opcua::AttributeId::Value};
            opcua::services::createMonitoredItemDataChangeAsync(
                client,
                *subscription_id_,
                read_value,
                opcua::MonitoringMode::Reporting,
                parameters,
                [this, node](opcua::IntegerId, opcua::IntegerId, const opcua::DataValue& value) noexcept {
                    try {
                        data_received_(node, value);
                    } catch (std::exception& ex) {
                        error_occurred_(ex.what());
                    }
                },
                [this, node](opcua::IntegerId, opcua::IntegerId) noexcept {
                    info_occurred_(std::format("Monitored item was deleted: {}", opcua::toString(node)));
                },
                [this, node](opcua::MonitoredItemCreateResult& result) noexcept {
                    try {
                        result.statusCode().throwIfBad();
                        monitored_item_ids_.push_back(result.monitoredItemId());

                        info_occurred_.invoke(std::format("Monitored item was created: {}", opcua::toString(node)));
                    } catch (const std::exception& error) {
                        error_occurred_.invoke(
                            std::format("Cannot monitor {}: {}", opcua::toString(node), error.what())
                        );
                    }
                }
            );
        }

    } catch (std::exception& ex) {
        error_occurred_.invoke(ex.what());
    }

    return {};
}

#pragma endregion

#pragma region Private methods

void Subscription::release(opcua::IntegerId subscription_id) noexcept {
    if (subscription_id_ == subscription_id) {
        subscription_id_.reset();
        monitored_item_ids_.clear();
    }
}

#pragma endregion
}  // namespace tpc::system::client
