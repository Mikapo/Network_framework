#pragma once

#include "../Utility/Net_common.h"
#include "../Utility/Net_message.h"
#include "../Utility/Thread_safe_deque.h"
#include <functional>
#include <iostream>
#include <memory>

namespace Net
{
    template <Id_concept Id_type>
    class Net_connection
    {
    public:
        Net_connection(Protocol::socket socket, const std::function<void(std::function<void()>)>& asio_job_callback)
            : m_socket(std::move(socket))
        {
            if (asio_job_callback)
                m_asio_job_callback = asio_job_callback;
            else
                throw std::invalid_argument("asio_job_callback was null");
        }

        virtual ~Net_connection() = default;
        Net_connection(const Net_connection&) = delete;
        Net_connection(Net_connection&&) = delete;
        Net_connection& operator=(const Net_connection&) = delete;
        Net_connection& operator=(Net_connection&&) = delete;

        void disconnect()
        {
            if (is_connected())
                m_asio_job_callback([this]() { m_socket.close(); });
        }

        [[nodiscard]] bool is_connected() const
        {
            return m_socket.is_open();
        }

        bool async_send_message(const Net_message<Id_type>& message)
        {
            m_asio_job_callback([this, message] {
                const bool is_writing_message = !m_out_queue.empty();

                m_out_queue.push_back(message);

                if (!is_writing_message)
                    async_write_header();
            });

            return true;
        }

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

            asio::async_connect(m_socket, endpoints, [this](asio::error_code error, Protocol::endpoint endpoint) {
                if (!error)
                    this->start_waiting_for_messages();
            });
        }

    private:
        void force_disconnect()
        {
            if (m_socket.is_open())
                m_socket.close();
        }

        bool validate_header(Net_message_header<Id_type> header) const noexcept
        {
            const bool validation_key_correct = header.m_validation_key == VALIDATION_KEY;
            return validation_key_correct;
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
                            force_disconnect();
                            return;
                        }

                        if (m_temp_message.m_header.m_size == 0)
                        {
                            m_temp_message.resize_body(0);
                            add_message_to_incoming_queue(m_temp_message);
                            async_read_header();
                            return;
                        }

                        m_temp_message.resize_body(m_temp_message.m_header.m_size);
                        async_read_body();
                    }
                    else
                        force_disconnect();
                });
        }

        void async_read_body()
        {
            asio::async_read(
                m_socket, asio::buffer(m_temp_message.m_body.data(), m_temp_message.m_body.size()),
                [this](asio::error_code error, size_t size) {
                    if (!error)
                    {
                        add_message_to_incoming_queue(m_temp_message);
                        async_read_header();
                    }
                    else
                        force_disconnect();
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
                        force_disconnect();
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
                        force_disconnect();
                });
        }

        virtual void add_message_to_incoming_queue(const Net_message<Id_type>& message) = 0;

        Protocol::socket m_socket;
        Net_message<Id_type> m_temp_message;
        Thread_safe_deque<Net_message<Id_type>> m_out_queue;
        std::function<void(std::function<void()>)> m_asio_job_callback;
        bool m_is_waiting_for_messages = false;
    };
} // namespace Net
