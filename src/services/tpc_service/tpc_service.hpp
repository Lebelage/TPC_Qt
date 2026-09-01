#pragma once
#include <string>
#include <mutex>
#include <memory>

#include <tpc.hpp>

#include "models/connection_parameters.hpp"


namespace tpc_qt::services {
    enum class ConnectionStatus {
        Connected,
        Inactive,
        Disconnected,
    };

    class TpcService {
    public:
        static TpcService &instance();

        TpcService(const TpcService &) = delete;

        TpcService &operator=(const TpcService &) = delete;

        TpcService(TpcService &&) = delete;

        TpcService &operator=(TpcService &&) = delete;

        ~TpcService();

    private:
        TpcService();

    public:
        [[nodiscard]]
        ConnectionStatus get_connection_status() const noexcept;

        [[nodiscard]] auto get_frame_request() -> std::optional<std::unordered_map<std::string, double>>;

        // [[nodiscard]]
        // ConnectionStatus get_sensors_name() const noexcept;

        void set_connection_parameters(std::string endpoint);

        bool connect_async(std::string endpoint);

        void disconnect_async();

    public:
        auto dispose() -> void;

    private:
        auto on_connection_state_changed(tpc::system::client::ConnectionState) -> void;

        auto on_client_initialization_data_received(tpc::system::models::DiscoveryResult) -> void;

    public:
        tpc::utilities::event_handler<tpc::system::client::ConnectionState> connection_state_changed_;
        tpc::utilities::event_handler<tpc::system::models::DiscoveryResult> initialization_data_received_;

    private:
        mutable std::mutex mutex_;

        tpc_qt::models::ConnectionParameters connection_parameters_{};

        tpc::system::models::DiscoveryResult initialization_data_;

        ConnectionStatus connection_status_{ConnectionStatus::Disconnected};

        std::unique_ptr<tpc::system::TPC> tpc_;

        std::unordered_map<std::string, double> last_received_frame;

        std::vector<std::uint8_t> handlers_ids_;
    };
}
