#include "tpc/system/client/client.hpp"

#include <exec/start_detached.hpp>
#include <expected>
#include <memory>
#include <numeric>
#include <cmath>
#include <limits>
#include <open62541pp/client.hpp>
#include <stdexec/execution.hpp>
#include <vector>

#include "tpc/core/definitions/client_definitions.hpp"
#include "tpc/system/client/helpers/opcua_browse_adapter.hpp"
#include "tpc/system/models/data.hpp"
namespace tpc::system::client {

#pragma region Fabric/Constructor

[[nodiscard]] std::expected<std::unique_ptr<Client>, std::string> Client::create(std::string endpoint) {
    try {
        return std::unique_ptr<Client>{new Client(std::move(endpoint))};
    } catch (const std::exception& error) {
        return std::unexpected{
            std::format("[{}]: Failed to create TPC client: {}", core::definitions::CLIENT_ERROR, error.what())
        };
    } catch (...) {
        return std::unexpected(
            std::format("[{}]: Failed to create TPC client: unknown error", core::definitions::CLIENT_ERROR)
        );
    }
}

Client::Client(std::string endpoint) : client_{std::make_unique<opcua::Client>()}, endpoint_(std::move(endpoint)) {
    auto subscription_result = Subscription::create();

    if (!subscription_result)
        throw std::runtime_error{subscription_result.error()};

    subscription_ = std::move(*subscription_result);

    (void)subscription_->data_received_.subscribe([this](opcua::NodeId channel, opcua::DataValue value) {
        on_subscription_data_received(channel, value);
    });

    (void)subscription_->error_occurred_.subscribe([this](std::string str) {
        on_subscription_error_occurred(str);
    });

    (void)subscription_->info_occurred_.subscribe([this](std::string str) {
        on_subscription_info_occurred(str);
    });

    auto frame_receiver_result = FrameReceiver::create(36);

    if (!frame_receiver_result)
        throw std::runtime_error{frame_receiver_result.error()};

    frame_receiver_ = std::move(*frame_receiver_result);
}

#pragma endregion

#pragma region Public methods

std::expected<bool, std::string> Client::connect_async() {
    bool expected = false;

    if (!running_.compare_exchange_strong(expected, true))
        return false;

    stop_requested_ = false;

    auto result = initialize_opcua_handlers();

    if (!result) {
        running_ = false;
        return std::unexpected{result.error()};
    }

    opcua_thread_ = std::jthread([this](std::stop_token stop_token) noexcept {
        try {
            if (!client_)
                throw std::runtime_error{"OPC UA client is not initialized"};

            if (endpoint_.empty())
                throw std::runtime_error{"OPC UA endpoint is empty"};

            client_->connectAsync(endpoint_);

            while (!stop_token.stop_requested() && !stop_requested_.load()) {
                client_->runIterate(kPollingIntervalMs);
            }

            if (client_->isConnected())
                client_->disconnect();
        } catch (const std::exception& ex) {
            error_occurred_.invoke(ex.what());
        } catch (...) {
            error_occurred_.invoke("Unknown error in the OPC UA worker");
        }

        running_ = false;
    });
    return true;
}

bool Client::is_running() const {
    return running_.load();
}

std::vector<std::string> Client::get_sensors_names() const {
    std::shared_lock lock{channels_info_mutex_};
    std::vector<std::string> names;
    names.reserve(channels_info_.nodes.size());

    for (const auto& name : channels_info_.nodes | std::views::values)
        names.push_back(name);

    return names;
}

void Client::stop() {
    stop_requested_ = true;
    opcua_thread_.request_stop();
}

auto Client::get_frame() -> std::optional<std::unordered_map<std::string, double>> {
    auto frame_result = frame_receiver_->get_frame();

    if (!frame_result)
        return std::nullopt;

    auto received_frame = std::move(*frame_result);
    std::unordered_map<std::string, double> frame;
    frame.reserve(received_frame.size());
    std::shared_lock lock{channels_info_mutex_};

    for (const auto& [node_id, value] : received_frame) {
        const auto channel = channels_info_.nodes.find(node_id);

        if (channel != channels_info_.nodes.end())
            frame.insert_or_assign(channel->second, value);
    }

    return frame;
}

auto Client::get_telemetry() const -> models::TelemetryFrame {
    const auto samples = frame_receiver_->get_samples();
    models::TelemetryFrame frame;
    std::shared_lock lock{channels_info_mutex_};
    for (const auto& [node, sample] : samples) {
        if (const auto channel = channels_info_.nodes.find(node); channel != channels_info_.nodes.end())
            frame.emplace(channel->second, sample);
    }
    return frame;
}

#pragma endregion

#pragma region Private methods

auto Client::initialize_opcua_handlers() -> std::expected<void, std::string> {
    if (!client_)
        return std::unexpected("Client is not initialized");

    client_->onSessionActivated([this] {
        if (auto result = initialize_monitored_items(); !result)
            error_occurred_.invoke(result.error());
        on_opcua_session_activated();
    });
    client_->onConnected([this] {
        on_opcua_connected();
    });
    client_->onInactive([this] {
        on_opcua_inactive();
    });
    client_->onDisconnected([this] {
        on_opcua_disconnected();
    });
    client_->onSessionClosed([this] {
        on_opcua_session_closed();
    });

    return {};
}

#pragma region Helpers

auto Client::find_child(const opcua::BrowseResult& result, std::string_view name, opcua::NodeClass expected_class)
    -> std::optional<opcua::NodeId> {
    for (const auto& reference : result.references()) {
        if (!reference.isForward() || reference.nodeClass() != expected_class || !reference.nodeId().isLocal())
            continue;

        if (reference.browseName().name() == name)
            return reference.nodeId().nodeId();
    }

    return std::nullopt;
}

auto Client::find_slot_ids(const opcua::BrowseResult& result) -> std::vector<opcua::NodeId> {
    std::vector<opcua::NodeId> ids;

    for (const auto& reference : result.references()) {
        if (!reference.isForward() || reference.nodeClass() != opcua::NodeClass::Object ||
            !reference.nodeId().isLocal())
            continue;

        if (reference.browseName().name().starts_with("Slot "))
            ids.push_back(reference.nodeId().nodeId());
    }
    return ids;
}

auto Client::append_channels(tpc::system::models::DiscoveryResult& result, const opcua::BrowseResult& slot_result)
    -> void {
    for (const auto& reference : slot_result.references()) {
        if (!reference.isForward() || reference.nodeClass() != opcua::NodeClass::Variable ||
            !reference.nodeId().isLocal()) {
            continue;
        }

        result.nodes.emplace(reference.nodeId().nodeId(), std::string{reference.browseName().name()});
    }
}

#pragma endregion

#pragma region Handlers

auto Client::on_subscription_data_received(opcua::NodeId node, opcua::DataValue value) -> void {
    models::ChannelSample sample{
        .received_at = std::chrono::steady_clock::now(),
        .source_time = value.hasSourceTimestamp() ? value.sourceTimestamp().toTimePoint<std::chrono::system_clock::duration>()
            : std::chrono::system_clock::time_point{},
        .good = value.hasValue() && value.status().isGood(),
        .has_source_time = value.hasSourceTimestamp()
    };
    try {
        const auto values = value.value().array<double>();
        sample.good = sample.good && !values.empty();
        // Incremental mean avoids overflowing the sum of otherwise finite samples.
        std::size_t count = 0;
        for (const double item : values) {
            sample.good = sample.good && std::isfinite(item);
            sample.value += (item - sample.value) / static_cast<double>(++count);
        }
        sample.good = sample.good && std::isfinite(sample.value);
    } catch (const std::exception&) {
        sample.good = false;
    }
    if (auto result = frame_receiver_->add_sample(node, sample); !result)
        warning_occurred_.invoke(result.error());
}

auto Client::on_subscription_error_occurred(std::string message) -> void {
    error_occurred_.invoke(std::format("[{}]: {}", core::definitions::CLIENT_ERROR, message));
}

auto Client::on_subscription_info_occurred(std::string message) -> void {
    info_occurred_.invoke(std::format("[{}]: {}", core::definitions::CLIENT_INFO, message));
}

auto Client::on_opcua_session_activated() -> void {
    connection_state_changed_.invoke(ConnectionState::SessionActivated);
}

auto Client::on_opcua_connected() -> void {
    connection_state_changed_.invoke(ConnectionState::Connected);
}

auto Client::on_opcua_inactive() -> void {
    connection_state_changed_.invoke(ConnectionState::Inactive);
}

auto Client::on_opcua_session_closed() -> void {
    connection_state_changed_.invoke(ConnectionState::SessionClosed);
}

auto Client::on_opcua_disconnected() -> void {
    connection_state_changed_.invoke(ConnectionState::Disconnected);
}

#pragma endregion

#pragma endregion

}  // namespace tpc::system::client
