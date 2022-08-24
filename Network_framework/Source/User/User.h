#pragma once

#include "../Connection/Connection.h"
#include "../Message/Message_converter.h"
#include "../Message/Owned_message.h"
#include "../Utility/Common.h"
#include "../Utility/Delegate.h"
#include "../Utility/Thread_safe_deque.h"
#include <chrono>
#include <concepts>
#include <optional>
#include <thread>

namespace Net
{
    // Base class for the server and the client
    template <Id_concept Id_type>
    class User
    {
    public:
        using Seconds = std::chrono::seconds;
        using Optional_seconds = std::optional<Seconds>;
        using Accepted_messages_container = std::unordered_map<Id_type, Message_limits>;

        User() : m_ssl_context(asio::ssl::context::sslv23)
        {
            m_accepted_messages = std::make_shared<Accepted_messages_container>();
        }

        virtual ~User() = default;

        User(const User&) = delete;
        User(User&&) = delete;
        User& operator=(const User&) = delete;
        User& operator=(User&&) = delete;

        // Sets ssl certificate chain file
        void set_ssl_certificate_chain_file(const std::string& path)
        {
            m_ssl_context.use_certificate_chain_file(path);
        }

        // Sets ssl private key file in .pem format
        void set_ssl_private_key_file(const std::string& path)
        {
            m_ssl_context.use_private_key_file(path, asio::ssl::context::pem);
        }

        // Sets ssl tmp dh file
        void set_ssl_tmp_dh_file(const std::string& path)
        {
            m_ssl_context.use_tmp_dh_file(path);
        }

        // Sets ssl verify file
        void set_ssl_verify_file(const std::string& path)
        {
            m_ssl_context.load_verify_file(path);
        }

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

        Delegate<std::string_view, Severity> m_on_notification;

    protected:
        template<typename Verify_type>
        void set_ssl_verify_mode(Verify_type new_verify_mode)
        {
            m_ssl_context.set_verify_mode(new_verify_mode);
        }

        // Sets ssl pasword callback
        template<typename Func_type>
        void set_ssl_password_callback(const Func_type& func)
        {
            m_ssl_context.set_password_callback(func);
        }

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

        /**
         *   Starts the asio thread and setups the Asio to handle async task'
         *
         *   @throws if the Asio thread was already running
         */
        void start_asio_thread()
        {
            if (!m_asio_thread_handle.joinable())
            {
                if (m_asio_context.stopped())
                    m_asio_context.restart();

                m_asio_thread_stop_flag = false;
                m_asio_thread_handle = std::thread([this] { asio_thread(); });
            }
            else
                throw std::logic_error("Asio thread was already running");
        }

        // Stops the Asio context and the Thread
        void stop_asio_thread()
        {
            if (!m_asio_thread_stop_flag)
            {
                m_asio_thread_stop_flag = true;
                m_asio_context.stop();

                if (m_asio_thread_handle.joinable())
                    m_asio_thread_handle.join();
            }
        }

        /*
        * Creates socket and connection for it
        * 
        * @param arguments for the connection
        * @retúrn unique_ptr to the connection obj
        */
        template<typename... Argtypes>
        [[nodiscard]] std::unique_ptr<Connection<Id_type>> create_connection(Argtypes... args)
        {
            Protocol::socket socket(m_asio_context);
            return create_connection_from_socket(std::move(socket), std::forward<Argtypes>(args)...);
        }

        /**
         *   Creates new connection object from socket
         *
         *   @param socket where to create the connection from
         *   @param the arguments for the connection constructor
         *   @return unique_ptr to the connection object
         */
        template <typename... Argtypes>
        [[nodiscard]] std::unique_ptr<Connection<Id_type>> create_connection_from_socket(
            Protocol::socket socket, Argtypes... Connection_constructor_arguments)
        {
            Encrypted_socket encrypted_socket(std::move(socket), m_ssl_context);

            std::unique_ptr new_connection =
                std::make_unique<Connection<Id_type>>(std::move(encrypted_socket), std::forward<Argtypes>(Connection_constructor_arguments)...);

            // Setups the callbacks
            new_connection->m_on_message.set_callback(
                [this](Owned_message<Id_type> message) { on_message_received(std::move(message)); });

            new_connection->m_on_notification.set_callback(
                [this](const std::string& message, Severity severity) { notifications_push_back(message, severity); });

            // Gives shared pointer of the accepted messages to the connection
            new_connection->set_accepted_messages(m_accepted_messages);

            return new_connection;
        }

        [[nodiscard]] Protocol::resolver create_resolver()
        {
            return Protocol::resolver(m_asio_context);
        }

        [[nodiscard]] Protocol::acceptor create_acceptor(const Protocol::endpoint& endpoint)
        {
            return Protocol::acceptor(m_asio_context, endpoint);
        }

        [[nodiscard]] virtual bool should_stop_waiting()
        {
            const bool has_messages = !m_in_queue.empty();
            const bool has_notifications = !m_notifications.empty();

            return has_messages || has_notifications;
        }

        // Async sends the message to the spesified connection
        void async_send_message_to_connection(Connection<Id_type>* connection, Message<Id_type> message)
        {
            give_job_to_asio([connection, moved_message = std::move(message)]() mutable {
                connection->send_message(std::move(moved_message));
            });
        }

        // Event when received new message from the connection
        void on_message_received(Owned_message<Id_type> message)
        {
            in_queue_push_back(std::move(message));
        }

    private:
        struct Notification
        {
            std::string m_message = "";
            Severity m_severity = Severity::notification;
        };

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

        // Seperate thread for running the Asio async
        void asio_thread()
        {
            while (!m_asio_thread_stop_flag)
                m_asio_context.run();
        }

        /**
         *   Gives async job to the Asio
         *
         *   @throws if the asio thread is not running
         */
        template <typename Func_type>
        void give_job_to_asio(Func_type job)
        {
            if (!m_asio_thread_stop_flag)
                asio::post(m_asio_context, std::forward<Func_type>(job));
            else
                throw std::logic_error("Asio thread was not running");
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

        asio::io_context m_asio_context;
        asio::ssl::context m_ssl_context;

        std::thread m_asio_thread_handle;
        bool m_asio_thread_stop_flag = true;

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
