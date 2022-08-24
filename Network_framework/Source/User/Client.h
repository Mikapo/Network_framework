#pragma once

#include "../Utility/Thread_safe_deque.h"
#include "User.h"
#include <cstdint>
#include <memory>

namespace Net
{
    template <Id_concept Id_type>
    class Client : public User<Id_type>
    {
    public:
        using Optional_seconds = std::optional<std::chrono::seconds>;

        Client() : m_temp_socket(this->create_encrypted_socket())
        {
            this->set_ssl_verify_mode(asio::ssl::context::verify_peer);
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
                /**
                 *   Using resolver allows more than just the ip being passed to this function.
                 *   For eexamble you can pass webpage addresses for conneting.
                 */
                m_has_received_server_data = false;
                Protocol::resolver resolver = this->create_resolver();
                auto endpoints = resolver.resolve(host, port);
                async_connect(endpoints);

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

        /**
         *   Handle everything received through internet
         *
         *   @param The max items handled
         *   @param Should the function wait if there is no items to handle
         *   @param Optional interval for checking connections. If you don't give this there will be no checking
         */
        void update(
            size_t max_items = SIZE_T_MAX, bool wait = false,
            Optional_seconds check_connections_interval = Optional_seconds()) override
        {
            User<Id_type>::update(max_items, wait, check_connections_interval);

            handle_received_messages(max_items);
        }

        // Sends the message to the server or does nothing if not connected
        void send_message(Message<Id_type> message)
        {
            if (is_connected())
                m_connection->send_message(std::move(message));
        }

        // You can only start sending messages to server after this event
        Delegate<> m_on_connected;

        Delegate<Message<Id_type>> m_on_message;

    private:
        void async_connect(Protocol::resolver::results_type endpoints)
        {
            m_temp_socket = this->create_encrypted_socket();
            asio::async_connect(
                m_temp_socket.lowest_layer(), endpoints,
                [this](asio::error_code error, const Protocol::endpoint& endpoint) {
                    if (!error)
                    {
                        m_connection = this->create_connection(std::move(m_temp_socket), 0, Handshake_type::client);
                    }
                    else
                        this->notifications_push_back(
                            std::format("Error on connection because {}", error.message()), Severity::error);
                });
        }

        // Triggers the on message callback for all the received messages
        void handle_received_messages(size_t max_messages)
        {
            for (size_t i = 0; i < max_messages && !this->is_in_queue_empty(); ++i)
            {
                auto owned_message = this->in_queue_pop_front();

                if (owned_message.m_message.get_internal_id() == Internal_id::not_internal)
                    m_on_message.broadcast(std::move(owned_message.m_message));
                else
                    handle_internal_message(std::move(owned_message));
            }
        }

        void handle_server_data(const Server_data& data)
        {
            m_remote_id = data.m_client_id;
            m_has_received_server_data = true;
            m_on_connected.broadcast();
        }

        // Handles the message that is internal to the framework
        void handle_internal_message(Owned_message<Id_type> owned_message)
        {
            Message<Id_type>& message = owned_message.m_message;

            switch (message.get_internal_id())
            {
            case Internal_id::server_data:
                handle_server_data(Message_converter<Id_type>::extract_server_data(message));
                break;
            default:
                break;
            }
        }

        Encrypted_socket m_temp_socket;
        std::unique_ptr<Connection<Id_type>> m_connection;

        uint32_t m_remote_id = 0;
        bool m_has_received_server_data = false;
    };
} // namespace Net
