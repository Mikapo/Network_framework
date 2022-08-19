#pragma once

#include "../Utility/Delegate.h"
#include "../Utility/Net_common.h"
#include "../Utility/Net_message.h"
#include "../Utility/Thread_safe_deque.h"
#include <functional>
#include <iostream>
#include <memory>
#include <unordered_map>

namespace Net
{
    struct Message_limits
    {
        uint32_t m_min = 0, m_max = 0;
    };

    template <Id_concept Id_type>
    class Net_connection
    {
    public:
        explicit Net_connection(Protocol::socket socket) : m_socket(std::move(socket))
        {
            update_ip();
        }

        virtual ~Net_connection() = default;
        Net_connection(const Net_connection&) = delete;
        Net_connection(Net_connection&&) = delete;
        Net_connection& operator=(const Net_connection&) = delete;
        Net_connection& operator=(Net_connection&&) = delete;

        void disconnect(const std::string& reason = "", bool is_error = false)
        {
            if (is_connected())
            {
                if (!reason.empty())
                    m_on_notification.broadcast(reason, is_error ? Severity::error : Severity::notification);

                m_socket.shutdown(asio::socket_base::shutdown_both);
                m_socket.close();
            }
        }

        [[nodiscard]] bool is_connected() const
        {
            return m_socket.is_open();
        }

        [[nodiscard]] std::string_view get_ip() const
        {
            return m_ip;
        }

        void send_message(const Net_message<Id_type>& message)
        {
            const bool is_writing_message = !m_out_queue.empty();

            m_out_queue.push_back(message);

            if (!is_writing_message)
                async_write_header();
        }

        void add_accepted_message(Id_type type, Message_limits limits)
        {
            m_accepted_messages[type] = limits;
        }

        void set_accepted_messages(const std::unordered_map<Id_type, Message_limits>& accepted_messages)
        {
            m_accepted_messages = accepted_messages;
        }

        Delegate<const std::string&, Severity> m_on_notification;
        Delegate<Owned_message<Id_type>> m_on_message;

    protected:
        void start_waiting_for_messages()
        {
            if (!m_is_waiting_for_messages)
            {
                async_read_header();
                m_is_waiting_for_messages = true;
            }
            else
                throw std::runtime_error("Was already waiting for messages");
        }

        void async_connect(const Protocol::resolver::results_type& endpoints)
        {
            if (is_connected())
                throw std::runtime_error("Socket is already connected");

            asio::async_connect(
                m_socket, endpoints, [this](asio::error_code error, const Protocol::endpoint& endpoint) {
                    if (!error)
                    {
                        update_ip();
                        m_on_notification.broadcast(
                            std::format("Connected sucesfully to {}", get_ip()), Severity::notification);
                        this->start_waiting_for_messages();
                    }
                });
        }

        void update_ip()
        {
            if (is_connected())
                m_ip = m_socket.remote_endpoint().address().to_string();
        }

    private:
        [[nodiscard]] bool validate_header(Net_message_header<Id_type> header) const noexcept
        {
            if (header.m_validation_key != Net_message_header<Id_type>::CONSTANT_VALIDATION_KEY)
                return false;

            auto found_limits = m_accepted_messages.find(header.m_id);

            if (found_limits == m_accepted_messages.end())
                return false;

            if (header.m_size < found_limits->second.m_min || header.m_size > found_limits->second.m_max)
                return false;

            return true;
        }

        void async_read_header()
        {
            m_temp_message.m_header.m_validation_key = 0;

            asio::async_read(
                m_socket, asio::buffer(&m_temp_message.m_header, sizeof(Net_message_header<Id_type>)),
                [this](asio::error_code error, size_t size) {
                    if (!error)
                    {
                        if (!validate_header(m_temp_message.m_header))
                        {
                            disconnect("Header validation failed", true);
                            return;
                        }

                        if (m_temp_message.m_header.m_size == 0)
                        {
                            m_temp_message.resize_body(0);
                            on_message_received(std::move(m_temp_message));
                            m_temp_message = Net_message<Id_type>();
                            async_read_header();
                            return;
                        }

                        m_temp_message.resize_body(m_temp_message.m_header.m_size);
                        async_read_body();
                    }
                    else
                        disconnect(error.message(), true);
                });
        }

        void async_read_body()
        {
            asio::async_read(
                m_socket, asio::buffer(m_temp_message.m_body.data(), m_temp_message.m_body.size()),
                [this](asio::error_code error, size_t size) {
                    if (!error)
                    {
                        on_message_received(std::move(m_temp_message));
                        m_temp_message = Net_message<Id_type>();
                        async_read_header();
                    }
                    else
                        disconnect(error.message(), true);
                });
        }

        void async_write_header()
        {
            asio::async_write(
                m_socket, asio::buffer(&m_out_queue.front().m_header, sizeof(Net_message_header<Id_type>)),
                [this](asio::error_code error, size_t size) {
                    if (!error)
                    {
                        if (m_out_queue.front().m_header.m_size > 0)
                            async_write_body();
                        else
                        {
                            m_out_queue.pop_front();

                            if (!m_out_queue.empty())
                                async_write_header();
                        }
                    }
                    else
                        disconnect(error.message(), true);
                });
        }

        void async_write_body()
        {
            asio::async_write(
                m_socket, asio::buffer(m_out_queue.front().m_body.data(), m_out_queue.front().m_body.size()),
                [this](asio::error_code error, size_t size) {
                    if (!error)
                    {
                        m_out_queue.pop_front();

                        if (!m_out_queue.empty())
                            async_write_header();
                    }
                    else
                        disconnect(error.message(), true);
                });
        }

        void on_message_received(Net_message<Id_type> message) const
        {
            Owned_message<Id_type> owned_message = create_owned_message(std::move(message));
            m_on_message.broadcast(std::move(owned_message));
        }

        [[nodiscard]] virtual Owned_message<Id_type> create_owned_message(Net_message<Id_type> message) const
        {
            return Owned_message<Id_type>(std::move(message), nullptr);
        }

        Protocol::socket m_socket;
        std::string m_ip = "0.0.0.0";
        bool m_is_waiting_for_messages = false;

        Net_message<Id_type> m_temp_message;
        Thread_safe_deque<Net_message<Id_type>> m_out_queue;
        std::unordered_map<Id_type, Message_limits> m_accepted_messages;
    };
} // namespace Net
