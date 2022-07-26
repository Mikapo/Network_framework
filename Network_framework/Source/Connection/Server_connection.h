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

        Server_connection(Protocol::socket socket, const std::function<void(std::function<void()>)>& asio_job_callback)
            : Net_connection(std::move(socket), asio_job_callback)
        {
        }

        void connect_to_server(const Protocol::resolver::results_type& endpoints)
        {
            this->async_connect(endpoints);
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
        }

        std::function<void(const Net_message<Id_type>&)> m_on_message_received_callback;
    };
} // namespace Net
