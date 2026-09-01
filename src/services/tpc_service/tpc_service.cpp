#include "services/tpc_service/tpc_service.hpp"

namespace tpc_qt::services {
#pragma region Constructor/Destructor
    TpcService &TpcService::instance() {
        static TpcService service;
        return service;
    }

    TpcService::TpcService() {
        auto result =
                tpc::system::TPC::create("opc.tcp://127.0.0.1:1234");

        if (!result)
            return;

        tpc_ = std::move(*result);

        tpc_->connection_state_changed_.subscribe([this](tpc::system::client::ConnectionState state) {
            on_connection_state_changed(state);
        });

        tpc_->initialization_data_received_.subscribe([this](tpc::system::models::DiscoveryResult result) {
            on_client_initialization_data_received(result);
        });
    }

    TpcService::~TpcService() {
        dispose();
    }

    auto TpcService::dispose() -> void {
        connection_state_changed_.dispose();
        disconnect_async();
    }
#pragma endregion

#pragma region Properties
    ConnectionStatus TpcService::get_connection_status() const noexcept {
        std::scoped_lock lock{mutex_};
        return connection_status_;
    }

    auto TpcService::get_frame_request() -> std::optional<std::unordered_map<std::string, double> > {
        auto result = tpc_->get_frame_request();
        if (!result)
            return std::nullopt;

        return result.value();
    }
#pragma endregion

#pragma region Public methods

    void TpcService::set_connection_parameters(std::string endpoint) {
        connection_parameters_.endpoint = endpoint;
    }

    bool TpcService::connect_async(std::string endpoint) {
        tpc_->start_async();
        return true;
    }

    void TpcService::disconnect_async() {
        if (tpc_)
            tpc_->stop_async();
    }

#pragma endregion

#pragma region Private methods
#pragma endregion

#pragma region Handlers
    auto TpcService::on_connection_state_changed(tpc::system::client::ConnectionState state) -> void {
        connection_state_changed_.invoke(state);
    }

    auto TpcService::on_client_initialization_data_received(
        tpc::system::models::DiscoveryResult discovery_result) -> void {
        initialization_data_ = discovery_result;
        initialization_data_received_.invoke(discovery_result);
    }

#pragma endergion
}
