#pragma once
#pragma once

#include "Net_connection.h"

namespace Net
{
    template <Id_concept Id_type>
    class Server_connection : public Net_connection<Id_type>
    {
    public:
        using Net_connection = Net_connection<Id_type>;

        explicit Server_connection(Protocol::socket socket) : Net_connection(std::move(socket))
        {
        }

        void connect_to_server(const Protocol::resolver::results_type& endpoints)
        {
            this->async_connect(endpoints);
        }

    private:
        void on_message_received(Net_message<Id_type> message) override
        {
            Owned_message<Id_type> owned_message(std::move(message), nullptr);
            this->broadcast_on_message_received(std::move(owned_message));
        }

        std::function<void(const Net_message<Id_type>&)> m_on_message_received_callback;
    };
} // namespace Net
