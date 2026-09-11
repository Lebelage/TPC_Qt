module;
#include <string>
#include <mutex>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <span>
export module tpc_qt.services.tpc_srvice;

import event_handler;
import tpc.system.client;
import tpc.system.models.system_data;
import tpc.system.tpc;
import tpc_qt.models.tpc_data_model;
export namespace tpc_qt::services {
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

        [[nodiscard]] auto get_frame_request() -> std::optional<std::unordered_map<std::string, double> >;

        [[nodiscard]] auto get_initialization_data() noexcept -> std::optional<tpc::system::models::DiscoveryResult&>;


        // [[nodiscard]]
        // ConnectionStatus get_sensors_name() const noexcept;

        void set_connection_parameters(std::string endpoint);

        bool connect_async(std::string endpoint);

        void disconnect_async();

        void calculate_field_3d(std::span<double> sensors_values, std::span<double> sensors_positions);

    public:
        auto dispose() -> void;

    private:
        auto on_connection_state_changed(tpc::system::client::ConnectionState) -> void;

        auto on_client_initialization_data_received(tpc::system::models::DiscoveryResult) -> void;

    public:
        tpc::utilities::event_handler<tpc::system::client::ConnectionState> connection_state_changed_;

    private:
        mutable std::mutex mutex_;

        models::TpcDataModel tpc_data_{};

        std::unique_ptr<tpc::system::TPC> tpc_;

        std::vector<std::uint8_t> handlers_ids_;

        ConnectionStatus connection_status_{ConnectionStatus::Disconnected};
    };
}
