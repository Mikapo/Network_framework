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
                /**
                 *   Using resolver allows more than just the ip being passed to this function.
                 *   For eexamble you can pass webpage addresses for conneting.
                 */
                Protocol::resolver resolver = this->create_resolver();
                auto endpoints = resolver.resolve(host, port);

                // Creates the connection and passes the endpoints to it for connecting
                m_connection = this->create_connection(this->create_socket(), 0, std::ref(endpoints));

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
                this->async_send_message_to_connection(m_connection.get(), std::move(message));
        }

        Delegate<Message<Id_type>> m_on_message;

    private:
        // Triggers the on message callback for all the received messages
        void handle_received_messages(size_t max_messages)
        {
            for (size_t i = 0; i < max_messages && !this->is_in_queue_empty(); ++i)
            {
                auto message = this->in_queue_pop_front();
                m_on_message.broadcast(std::move(message.m_message));
            }
        }

        std::unique_ptr<Connection<Id_type>> m_connection;
    };
} // namespace Net
