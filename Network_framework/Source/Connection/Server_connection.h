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

        Server_connection(asio::io_context& io_context, Protocol::socket socket)
            : Net_connection(io_context, std::move(socket))
        {
        }

        void connect_to_server(const Protocol::resolver::results_type& endpoints)
        {
            asio::async_connect(this->m_socket, endpoints, [this](asio::error_code error, Protocol::endpoint endpoint) {
                if (!error)
                    this->async_read_header();
            });
        }

        template <typename Func_type>
        void set_on_message_received_callback(const Func_type& func)
        {
            m_on_message_received_callback = func;
        }

    private:
        void add_message_to_incoming_queue(const Net_message<Id_type>& message) override
        {
            this->m_on_message_received_callback(message);
            this->async_read_header();
        }

        std::function<void(const Net_message<Id_type>&)> m_on_message_received_callback;
    };
} // namespace Net
