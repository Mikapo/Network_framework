#pragma once

#include "Net_connection.h"
#include <memory>

namespace Net
{
    template <Id_concept Id_type>
    class Client_connection : public Net_connection<Id_type>,
                              public std::enable_shared_from_this<Client_connection<Id_type>>
    {
    public:
        using Client_connection_ptr = std::shared_ptr<Client_connection<Id_type>>;
        using Net_connection = Net_connection<Id_type>;

        Client_connection(asio::io_context& io_context, Protocol::socket socket)
            : Net_connection(io_context, std::move(socket))
        {
        }

        void connect_to_client(uint32_t id)
        {
            if (this->m_socket.is_open())
            {
                m_id = id;
                this->async_read_header();
            }
        }

        uint32_t get_id() const noexcept
        {
            return m_id;
        }

        template <typename Func_type>
        void set_on_message_received_callback(const Func_type& func)
        {
            m_on_message_received_callback = func;
        }

    private:
        void add_message_to_incoming_queue(const Net_message<Id_type>& message) override
        {
            if (this->m_on_message_received_callback)
                this->m_on_message_received_callback(message, this->shared_from_this());
        }

        std::function<void(const Net_message<Id_type>&, Client_connection_ptr)> m_on_message_received_callback;
        uint32_t m_id = 0;
    };
} // namespace Net
