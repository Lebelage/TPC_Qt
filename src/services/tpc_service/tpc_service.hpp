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

    private:
        TpcService();

    public:
        [[nodiscard]]
        ConnectionStatus get_connection_status() const noexcept;

        // [[nodiscard]]
        // bool connected() const noexcept;
        //
        // [[nodiscard]]
        // std::string endpoint() const;

        void set_connection_parameters(std::string endpoint);

        bool connect_async(std::string endpoint);

        void disconnect_async();

    private:
        auto on_connection_state_changed(tpc::system::client::ConnectionState) -> void;

    public:
        tpc::utilities::event_handler<tpc::system::client::ConnectionState> connection_state_changed_;

    private:
        mutable std::mutex mutex_;

        tpc_qt::models::ConnectionParameters connection_parameters_{};

        ConnectionStatus connection_status_{ConnectionStatus::Disconnected};

        std::unique_ptr<tpc::system::TPC> tpc_;

        std::vector<std::uint8_t> handlers_ids_;
    };
}
