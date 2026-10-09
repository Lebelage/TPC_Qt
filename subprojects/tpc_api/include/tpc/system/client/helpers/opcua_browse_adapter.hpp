#pragma once

#include <open62541pp/client.hpp>
#include <stdexec/__detail/__operation_states.hpp>
#include <stdexec/__detail/__receivers.hpp>
#include <stdexec/execution.hpp>

#include "open62541pp/services/view.hpp"
namespace tpc::system::client::helpers {
template <stdexec::receiver Receiver> struct browse_operation {
    using operation_state = stdexec::operation_state_tag;

    opcua::Client* client_;
    opcua::NodeId node_id_;
    Receiver receiver_;

    void start() noexcept {
        try {
            const opcua::BrowseDescription desc{std::move(node_id_), opcua::BrowseDirection::Forward,
                                                opcua::ReferenceTypeId::References};

            opcua::services::browseAsync(*client_, desc, 0, [this](opcua::BrowseResult& result) noexcept {
                stdexec::set_value(std::move(receiver_), std::move(result));
            });
        } catch (...) {
            stdexec::set_error(std::move(receiver_), std::current_exception());
        }
    }
};

struct browse_sender {
    using sender_concept = stdexec::sender_tag;
    using completion_signatures = stdexec::completion_signatures<stdexec::set_value_t(opcua::BrowseResult),
                                                                 stdexec::set_error_t(std::exception_ptr)>;

    opcua::Client* client_;
    opcua::NodeId node_id_;

    template <stdexec::receiver_of<completion_signatures> Receiver> auto connect(Receiver receiver) && {
        return browse_operation<Receiver>{client_, std::move(node_id_), std::move(receiver)};
    }
};

[[nodiscard]] inline auto browse_async(opcua::Client& client, opcua::NodeId node_id) {
    return browse_sender{&client, std::move(node_id)};
}
} // namespace tpc::system::client::helpers
