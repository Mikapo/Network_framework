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
        using Server_connection_ptr = std::shared_ptr<Server_connection>;

        Client() noexcept
        {
        }

        ~Client() override
        {
            disconnect();
        }

        Client(const Client&) = delete;
        Client(Client&&) = delete;
        Client& operator=(const Client&) = delete;
        Client& operator=(Client&&) = delete;

        bool connect(std::string_view host, std::string_view port)
        {
            try
            {
                Protocol::resolver resolver = this->create_resolver();
                auto endpoints = resolver.resolve(host, port);

                m_connection = this->create_connection<Server_connection>(this->create_socket());
                m_connection->connect_to_server(endpoints);
                this->start_asio_thread();
            }
            catch (const std::exception& exception)
            {
                this->notifications_push_back(std::format("Exception: {}", exception.what()), Severity::error);
                return false;
            }

            return true;
        }

        void disconnect()
        {
            this->stop_asio_thread();

            if (is_connected())
                m_connection->disconnect();

            m_connection.reset();
        }

        [[nodiscard]] bool is_connected() const
        {
            if (m_connection)
                return m_connection->is_connected();

            return false;
        }

        void update(
            size_t max_messages = std::numeric_limits<size_t>::max(), bool wait = false,
            std::optional<std::chrono::seconds> check_connections_interval =
                std::optional<std::chrono::seconds>()) override
        {
            Net_user<Id_type>::update(max_messages, wait, check_connections_interval);

            handle_received_messages(max_messages);
        }

        void send_message(const Net_message<Id_type>& message)
        {
            if (is_connected())
                this->async_send_message_to_connection(m_connection.get(), message);
        }

        Delegate<Net_message<Id_type>> m_on_message;

    private:
        void handle_received_messages(size_t max_messages)
        {
            for (size_t i = 0; i < max_messages && !this->is_in_queue_empty(); ++i)
            {
                auto message = this->in_queue_pop_front();
                m_on_message.broadcast(std::move(message.m_message));
            }
        }

        void on_new_accepted_message(Id_type type, Message_limits limits) override
        {
            if (m_connection)
                m_connection->add_accepted_message(type, limits);
        }

        Server_connection_ptr m_connection;
    };
} // namespace Net
