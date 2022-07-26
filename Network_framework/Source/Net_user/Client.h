#pragma once

#include "../Connection/Server_connection.h"
#include "../Utility/Thread_safe_deque.h"
#include "Net_user.h"
#include <cstdint>
#include <memory>

namespace Net
{
    template <Id_concept Id_type>
    class Client : public Net_user<Id_type>
    {
    public:
        using Server_connection = Server_connection<Id_type>;
        using Server_connection_ptr = std::unique_ptr<Server_connection>;

        Client() noexcept : m_socket(this->create_socket())
        {
        }

        virtual ~Client()
        {
            disconnect();
        }

        bool connect(std::string_view host, std::string_view port)
        {
            try
            {
                Protocol::resolver resolver = this->create_resolver();
                auto endpoints = resolver.resolve(host, port);

                auto give_asio_job_lambda = [this](std::function<void()> job) { this->give_asio_job(job); };
                m_connection = std::make_unique<Server_connection>(this->create_socket(), give_asio_job_lambda);

                m_connection->set_on_message_received_callback(
                    [this](const Net_message<Id_type>& message) { on_message_received(message); });

                m_connection->connect_to_server(endpoints);
                this->start_asio_thread();
            }
            catch (std::exception exception)
            {
                return false;
            }

            return true;
        }

        void disconnect()
        {
            if (is_connected())
                m_connection->disconnect();

            this->stop_asio_thread();
            m_connection.reset();
        }

        bool is_connected() const
        {
            if (m_connection)
                return m_connection->is_connected();
            else
                return false;
        }

        void send_message(const Net_message<Id_type>& message)
        {
            if (is_connected())
                m_connection->async_send_message(message);
        }

        void handle_received_messages(size_t max_messages = std::numeric_limits<size_t>::max())
        {
            for (size_t i = 0; i < max_messages && !this->is_in_queue_empty(); ++i)
            {
                auto message = this->in_queue_pop_front();
                on_message(message.m_message);
            }
        }

    protected:
        virtual void on_message(Net_message<Id_type>& message)
        {
        }

    private:
        void on_message_received(const Net_message<Id_type>& message)
        {
            Owned_message<Id_type> owned_message = {.m_owner = nullptr, .m_message = message};
            this->in_queue_push_back(std::move(owned_message));
        }

        Protocol::socket m_socket;
        Server_connection_ptr m_connection;
    };
} // namespace Net
