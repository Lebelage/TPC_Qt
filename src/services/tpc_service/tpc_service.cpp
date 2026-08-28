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
    }
#pragma endregion

#pragma region Properties
    ConnectionStatus TpcService::get_connection_status() const noexcept {
        std::scoped_lock lock{mutex_};
        return connection_status_;
    }

    // bool TpcService::connected() const noexcept {
    //     std::scoped_lock lock{mutex_};
    //     return state_ == State::Connected;
    // }
    //
    // std::string TpcService::endpoint() const {
    //     std::scoped_lock lock{mutex_};
    //     return endpoint_;
    // }

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
        tpc_->stop_async();
    }
#pragma endregion

#pragma region Private methods
#pragma endregion

#pragma region Handlers
    auto TpcService::on_connection_state_changed(tpc::system::client::ConnectionState state) -> void {
        connection_state_changed_.invoke(state);
    }

#pragma endergion
}
