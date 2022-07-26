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

        Client_connection(Protocol::socket socket, const std::function<void(std::function<void()>)>& asio_job_callback)
            : Net_connection(std::move(socket), asio_job_callback)
        {
        }

        void connect_to_client(uint32_t id)
        {
            if (this->is_connected())
            {
                m_id = id;
                this->start_waiting_for_messages();
            }
        }

        [[nodiscard]] uint32_t get_id() const noexcept
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
