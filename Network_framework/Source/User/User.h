#pragma once

#include "../Connection/Connection.h"
#include "../Sockets/Socket.h"
#include "../Message/Message_converter.h"
#include "../Message/Owned_message.h"
#include "../Utility/Delegate.h"
#include "../Utility/Thread_safe_deque.h"
#include "Asio_base.h"
#include <chrono>
#include <concepts>
#include <optional>
#include <thread>

namespace Net
{
    // Base class for the server and the client
    template <Id_concept Id_type>
    class User : public Asio_base
    {
    public:
        using Seconds = std::chrono::seconds;
        using Optional_seconds = std::optional<Seconds>;
        using Accepted_messages_container = std::unordered_map<Id_type, Message_limits>;

        User()
        {
            m_accepted_messages = std::make_shared<Accepted_messages_container>();
        }

        virtual ~User() = default;

        User(const User&) = delete;
        User(User&&) = delete;
        User& operator=(const User&) = delete;
        User& operator=(User&&) = delete;

        /**
         *   Add message id that gets accepted. By default, all messages id's are not accepted
         *   and if you receive an unaccepted message, you get disconnected.
         *   This also allows you to put size limits on the messages.
         *
         *   @param the type to be accepted
         *   @param the min message size
         *   @param the max message size
         */
        void add_accepted_message(Id_type type, uint32_t min = 0, uint32_t max = std::numeric_limits<uint32_t>::max())
        {
            const Message_limits limits = {.m_min = min, .m_max = max};
            m_accepted_messages->emplace(type, limits);
        }

        /**
         *   Handle everything received through internet
         *
         *   @param the max items handled
         *   @param should the function wait if there is no items to handle
         *   @param optional interval for checking connections. If you don't give this there will be no checking
         */
        virtual void update(
            size_t max_handled_items = SIZE_T_MAX, bool wait = false,
            Optional_seconds check_connections_interval = Optional_seconds())
        {
            if (check_connections_interval.has_value())
                handle_check_connections_delay(wait, check_connections_interval.value());
            else if (wait)
                wait_until_has_something_to_do();

            for (size_t i = 0; i < max_handled_items && !m_notifications.empty(); ++i)
            {
                const Notification notification = m_notifications.pop_front();
                m_on_notification.broadcast(notification.m_message, notification.m_severity);
            }
        }

        /** 
        *   Enables or disables ssl for the new connections.
        *   All old conecttions does not change.
        */
        void enable_ssl(bool enabled) noexcept
        {
            m_use_ssl = enabled;
        }

        Delegate<std::string_view, Severity> m_on_notification;

    protected:
        bool is_in_queue_empty()
        {
            return m_in_queue.empty();
        }

        /**
         * Thread safe pop_front
         *
         * @return message from in queue
         */
        [[nodiscard]] Owned_message<Id_type> in_queue_pop_front()
        {
            return m_in_queue.pop_back();
        }

        void notify_wait() noexcept
        {
            m_wait_condition.notify_one();
        }

        // Thread safe push back to queue
        void in_queue_push_back(Owned_message<Id_type> message)
        {
            m_in_queue.push_back(std::move(message));
            notify_wait();
        }

        // Thread safe push back to queue
        void notifications_push_back(std::string message, Severity severity = Severity::notification)
        {
            if (!m_on_notification.has_been_set())
                return;

            Notification notification = {.m_message = message, .m_severity = severity};
            m_notifications.push_back(std::move(notification));
            notify_wait();
        }
    
        [[nodiscard]] virtual bool should_stop_waiting()
        {
            const bool has_messages = !m_in_queue.empty();
            const bool has_notifications = !m_notifications.empty();

            return has_messages || has_notifications;
        }

      
        // Event when received new message from the connection
        void on_message_received(Owned_message<Id_type> message)
        {
            in_queue_push_back(std::move(message));
        }

        /**
         *   Creates new connection object from socket
         *
         *   @param socket to use
         *   @param if for rhe connection
         *   @param should we use client or server type of handshake
         *   @return unique_ptr to the connection object
         */
        [[nodiscard]] std::unique_ptr<Connection<Id_type>> create_connection(
            std::unique_ptr<Socket_interface> socket, uint32_t connection_id, Handshake_type handshake_type)
        {
            std::unique_ptr new_connection =
                std::make_unique<Connection<Id_type>>(std::move(socket), connection_id);

            // Setups the callbacks
            new_connection->m_on_message.set_callback(
                [this](Owned_message<Id_type> message) { on_message_received(std::move(message)); });

            new_connection->m_on_notification.set_callback(
                [this](const std::string& message, Severity severity) { notifications_push_back(message, severity); });

            // Gives shared pointer of the accepted messages to the connection
            new_connection->set_accepted_messages(m_accepted_messages);

            new_connection->start(handshake_type);

            return new_connection;
        }

        [[nodiscard]] std::unique_ptr<Connection<Id_type>> create_connection(
            Protocol::socket socket, uint32_t connection_id, Handshake_type handshake_type)
        {
            std::unique_ptr<Socket_interface> socket_interface = create_socket_interface(std::move(socket));
            return create_connection(std::move(socket_interface), connection_id, handshake_type);
        }

    private:
        struct Notification
        {
            std::string m_message = "";
            Severity m_severity = Severity::notification;
        };

        // Creates spesific socket interface for connection
        template<typename... args>
        [[nodiscard]] std::unique_ptr<Socket_interface> create_socket_interface(Protocol::socket socket)
        {
            if (m_use_ssl)
                return std::make_unique<Template_socket<Encrypted_socket>>(create_encrypted_socket(std::move(socket)));
            else
                return std::make_unique<Template_socket<Protocol::socket>>(std::move(socket));
        }

        // You can spesify the max waiting time otherwise this will wait until something notifies it
        void wait_until_has_something_to_do(std::optional<Seconds> wait_time = std::optional<Seconds>())
        {
            std::unique_lock lock(m_wait_mutex);

            auto wait_lambda = [this] { return should_stop_waiting(); };

            if (wait_time.has_value())
                m_wait_condition.wait_for(lock, wait_time.value(), wait_lambda);
            else
                m_wait_condition.wait(lock, wait_lambda);
        }

        /**
         * Checks if enough time has passed for checking all connections
         * Otherwise will wait if needed.
         *
         * @param should wait
         * @param interval between connection checks
         */
        void handle_check_connections_delay(bool wait, Seconds interval)
        {
            using namespace std::chrono;

            steady_clock::time_point time_now = high_resolution_clock::now();
            auto time_since_last_check = duration_cast<Seconds>(time_now - m_last_connection_check);

            if (wait)
            {
                auto wait_time = interval - time_since_last_check;
                wait_until_has_something_to_do(wait_time);

                if (wait_time > Seconds(0))
                {
                    time_now = high_resolution_clock::now();
                    time_since_last_check = duration_cast<Seconds>(time_now - m_last_connection_check);
                }
            }

            if (time_since_last_check >= interval)
            {
                check_connections();
                m_last_connection_check = high_resolution_clock::now();
            }
        }

        virtual void check_connections(){};

        bool m_use_ssl = false;

        std::condition_variable m_wait_condition;
        std::mutex m_wait_mutex;
        std::chrono::steady_clock::time_point m_last_connection_check;

        // Accepted message types
        std::shared_ptr<Accepted_messages_container> m_accepted_messages;

        // Received messages from the conenctions
        Thread_safe_deque<Owned_message<Id_type>> m_in_queue;

        // the notification to be handled
        Thread_safe_deque<Notification> m_notifications;
    };
}; // namespace Net
