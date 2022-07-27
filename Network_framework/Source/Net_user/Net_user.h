#pragma once

#include "../Connection/Net_connection.h"
#include "../Utility/Net_common.h"
#include "../Utility/Net_message.h"
#include "../Utility/Thread_safe_deque.h"
#include <concepts>
#include <functional>
#include <thread>

namespace Net
{
    template <Id_concept Id_type>
    class Net_user
    {
    public:
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

    protected:
        virtual void on_notification(std::string_view notification, Severity severity = Severity::notification)
        {
        }

        bool is_in_queue_empty()
        {
            return m_in_queue.empty();
        }

        Owned_message<Id_type> in_queue_pop_front()
        {
            return m_in_queue.pop_back();
        }

        void in_queue_push_back(const Owned_message<Id_type>& message)
        {
            m_in_queue.push_back(message);
            m_wait_until_messages.notify_one();
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
                [this](const Owned_message<Id_type>& message) { on_message_received(message); });

            new_connection->set_notification_callback(
                [this](const std::string_view message, Severity severity) { on_notification(message, severity); });

            new_connection->set_accepted_messages(get_current_accepted_messages());

            return new_connection;
        }

        Protocol::socket create_socket()
        {
            return Protocol::socket(m_asio_context);
        }

        Protocol::resolver create_resolver()
        {
            return Protocol::resolver(m_asio_context);
        }

        Protocol::acceptor create_acceptor(const Protocol::endpoint& endpoint)
        {
            return Protocol::acceptor(m_asio_context, endpoint);
        }

        void wait_until_has_messages()
        {
            std::unique_lock lock(m_wait_mutex);

            auto wait_lambda = [this] { return !m_in_queue.empty(); };

            m_wait_until_messages.wait(lock, wait_lambda);
        }

        template <typename Func_type>
        void give_job_to_asio(Func_type job)
        {
            if (!m_asio_thread_stop_flag)
                asio::post(m_asio_context, job);
            else
                throw std::runtime_error("asio thread was not running");
        }

        void async_send_message_to_connection(Net_connection<Id_type>* connection, const Net_message<Id_type>& message)
        {
            give_job_to_asio([this, connection, message] { connection->send_message(message); });
        }

        [[nodiscard]] const std::unordered_map<Id_type, Message_limits>& get_current_accepted_messages() const
        {
            return m_accepted_messages;
        }

        void on_message_received(const Owned_message<Id_type>& message)
        {
            this->in_queue_push_back(message);
        }

    private:
        void asio_thread()
        {
            while (!m_asio_thread_stop_flag)
                m_asio_context.run();
        }

        virtual void on_new_accepted_message(Id_type type, Message_limits limits) = 0;

        asio::io_context m_asio_context;

        std::thread m_thread_handle;
        bool m_asio_thread_stop_flag = true;

        std::condition_variable m_wait_until_messages;
        std::mutex m_wait_mutex;

        std::unordered_map<Id_type, Message_limits> m_accepted_messages;
        Thread_safe_deque<Owned_message<Id_type>> m_in_queue;
    };
}; // namespace Net
