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
        using Protocol_type = asio::ip::tcp;
        using Socket_type = Protocol_type::socket;

        Net_connection(asio::io_context& io_context, Socket_type socket)
            : m_socket(std::move(socket)), m_io_context(io_context)
        {
        }

        virtual ~Net_connection()
        {
        }

        void disconnect()
        {
            if (is_connected())
                asio::post(m_io_context, [this] { m_socket.close(); });
        }

        bool is_connected() const noexcept
        {
            return m_socket.is_open();
        }

        bool async_send_message(const Net_message<Id_type>& message)
        {
            asio::post(m_io_context, [this, message] {
                const bool is_writing_message = !m_out_queue.empty();

                m_out_queue.push_back(message);

                if (!is_writing_message)
                    async_write_header();
            });

            return true;
        }

    protected:
        void async_read_header()
        {
            asio::async_read(
                m_socket, asio::buffer(&m_temp_message.m_header, sizeof(Net_message_header<Id_type>)),
                [this](asio::error_code error, size_t size) {
                    if (!error)
                    {
                        std::cout << "reading header validation: " << m_temp_message.m_header.m_validation_key
                                  << " size: " << m_temp_message.m_header.m_size
                                  << " header bytes: " << sizeof(Net_message_header<Id_type>) << "\n";

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

        Socket_type m_socket;
        asio::io_context& m_io_context;

    private:
        void force_disconnect()
        {
            if (m_socket.is_open())
                m_socket.close();
        }

        bool validate_header(Net_message_header<Id_type> header)
        {
            const bool validation_key_correct = header.m_validation_key == VALIDATION_KEY;
            return validation_key_correct;
        }

        void async_read_body()
        {
            asio::async_read(
                m_socket, asio::buffer(m_temp_message.m_body.data(), m_temp_message.m_body.size()),
                [this](asio::error_code error, size_t size) {
                    if (!error)
                    {
                        std::cout << "reading body size:" << m_temp_message.m_body.size() << "\n";
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
                        std::cout << "writing header validation: " << m_out_queue.front().m_header.m_validation_key
                                  << " size: " << m_out_queue.front().m_header.m_size
                                  << " header bytes: " << sizeof(Net_message_header<Id_type>) << "\n";

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
                m_socket, asio::buffer(m_out_queue.front().m_body.data(), m_out_queue.front().m_header.m_size),
                [this](asio::error_code error, size_t size) {
                    if (!error)
                    {
                        std::cout << "Writing body size:" << m_out_queue.front().m_header.m_size << "\n";
                        m_out_queue.pop_front();

                        if (!m_out_queue.empty())
                            async_write_header();
                    }
                    else
                        force_disconnect();
                });
        }

        virtual void add_message_to_incoming_queue(const Net_message<Id_type>& message) = 0;

        Net_message<Id_type> m_temp_message;
        Thread_safe_deque<Net_message<Id_type>> m_out_queue;
    };
} // namespace Net
