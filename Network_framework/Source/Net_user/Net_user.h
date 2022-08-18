#pragma once

#include "../Connection/Net_connection.h"
#include "../Utility/Net_common.h"
#include "../Utility/Net_message.h"
#include "../Utility/Thread_safe_deque.h"
#include <chrono>
#include <concepts>
#include <functional>
#include <optional>
#include <thread>

namespace Net
{
    template <Id_concept Id_type>
    class Net_user
    {
    public:
        using Seconds = std::chrono::seconds;

        Net_user() = default;
        virtual ~Net_user() = default;

        Net_user(const Net_user&) = delete;
        Net_user(Net_user&&) = delete;
        Net_user& operator=(const Net_user&) = delete;
        Net_user& operator=(Net_user&&) = delete;

        void add_accepted_message(Id_type type, uint32_t min, uint32_t max)
        {
            const Message_limits limits = {.m_min = min, .m_max = max};
            m_accepted_messages[type] = limits;
            on_new_accepted_message(type, limits);
        }

        virtual void update(
            size_t max_handled_items = std::numeric_limits<size_t>::max(), bool wait = false,
            std::optional<Seconds> check_connections_interval = std::optional<Seconds>())
        {
            if (check_connections_interval.has_value())
                handle_check_connections_delay(wait, check_connections_interval.value());
            else if (wait)
                wait_until_has_something_to_do();

            for (size_t i = 0; i < max_handled_items && !m_notifications.empty(); ++i)
            {
                Notification notification = m_notifications.pop_front();
                on_notification(notification.m_message, notification.m_severity);
            }
        }

    protected:
        virtual void on_notification(std::string_view notification, Severity severity = Severity::notification)
        {
        }

        bool is_in_queue_empty()
        {
            return m_in_queue.empty();
        }

        [[nodiscard]] Owned_message<Id_type> in_queue_pop_front()
        {
            return m_in_queue.pop_back();
        }

        void notify_wait()
        {
            m_wait_condition.notify_one();
        }

        void in_queue_push_back(Owned_message<Id_type> message)
        {
            m_in_queue.push_back(std::move(message));
            notify_wait();
        }

        void notifications_push_back(const std::string& message, Severity severity = Severity::notification)
        {
            Notification notification = {.m_message = message, .m_severity = severity};
            m_notifications.push_back(std::move(notification));
            notify_wait();
        }

        void start_asio_thread()
        {
            if (!m_thread_handle.joinable())
            {
                if (m_asio_context.stopped())
                    m_asio_context.restart();

                m_asio_thread_stop_flag = false;
                m_thread_handle = std::thread([this] { asio_thread(); });
            }
            else
                throw std::runtime_error("asio thread is already running");
        }

        void stop_asio_thread()
        {
            if (!m_asio_thread_stop_flag)
            {
                m_asio_thread_stop_flag = true;
                m_asio_context.stop();

                if (m_thread_handle.joinable())
                    m_thread_handle.join();
            }
        }

        template <std::derived_from<Net_connection<Id_type>> Connection_type>
        [[nodiscard]] std::shared_ptr<Connection_type> create_connection(Protocol::socket socket)
        {
            std::shared_ptr new_connection = std::make_shared<Connection_type>(std::move(socket));

            new_connection->set_on_message_received_callback(
                [this](Owned_message<Id_type> message) { on_message_received(std::move(message)); });

            new_connection->set_notification_callback(
                [this](const std::string& message, Severity severity) { notifications_push_back(message, severity); });

            new_connection->set_accepted_messages(get_current_accepted_messages());

            return new_connection;
        }

        [[nodiscard]] Protocol::socket create_socket()
        {
            return Protocol::socket(m_asio_context);
        }

        [[nodiscard]] Protocol::resolver create_resolver()
        {
            return Protocol::resolver(m_asio_context);
        }

        [[nodiscard]] Protocol::acceptor create_acceptor(const Protocol::endpoint& endpoint)
        {
            return Protocol::acceptor(m_asio_context, endpoint);
        }

        [[nodiscard]] virtual bool should_stop_wait() noexcept
        {
            const bool has_messages = !m_in_queue.empty();
            const bool has_notifications = !m_notifications.empty();

            return has_messages || has_notifications;
        }

        void async_send_message_to_connection(Net_connection<Id_type>* connection, const Net_message<Id_type>& message)
        {
            give_job_to_asio([connection, message] { connection->send_message(message); });
        }

        [[nodiscard]] const std::unordered_map<Id_type, Message_limits>& get_current_accepted_messages() const
        {
            return m_accepted_messages;
        }

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

        void wait_until_has_something_to_do(std::optional<Seconds> wait_time = std::optional<Seconds>())
        {
            std::unique_lock lock(m_wait_mutex);

            auto wait_lambda = [this] { return should_stop_wait(); };

            if (wait_time.has_value())
                m_wait_condition.wait_for(lock, wait_time.value(), wait_lambda);
            else
                m_wait_condition.wait(lock, wait_lambda);
        }

        void asio_thread()
        {
            while (!m_asio_thread_stop_flag)
                m_asio_context.run();
        }

        template <typename Func_type>
        void give_job_to_asio(Func_type job)
        {
            if (!m_asio_thread_stop_flag)
                asio::post(m_asio_context, job);
            else
                throw std::runtime_error("asio thread was not running");
        }

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

        virtual void on_new_accepted_message(Id_type type, Message_limits limits) = 0;
        virtual void check_connections(){};

        asio::io_context m_asio_context;

        std::thread m_thread_handle;
        bool m_asio_thread_stop_flag = true;

        std::condition_variable m_wait_condition;
        std::mutex m_wait_mutex;
        std::chrono::steady_clock::time_point m_last_connection_check;

        std::unordered_map<Id_type, Message_limits> m_accepted_messages;
        Thread_safe_deque<Owned_message<Id_type>> m_in_queue;
        Thread_safe_deque<Notification> m_notifications;
    };
}; // namespace Net
