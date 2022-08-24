#pragma once

#include "../Message/Owned_message.h"
#include "../Utility/Common.h"
#include "../Utility/Delegate.h"
#include "../Utility/Thread_safe_deque.h"
#include <unordered_map>

namespace Net
{
    struct Message_limits
    {
        uint32_t m_min = 0, m_max = 0;
    };

    enum class Handshake_type : uint8_t
    {
        client,
        server
    };

    // Class that repesents remote net connection
    template <Id_concept Id_type>
    class Connection
    {
    public:
        using Accepted_messages_ptr = std::shared_ptr<const std::unordered_map<Id_type, Message_limits>>;
        using End_points = Protocol::resolver::results_type;

        Connection(Encrypted_socket socket, uint32_t connection_id, Handshake_type handshake_type)
            : m_id(connection_id), m_handshake_type(handshake_type), m_encrypted_socket(std::move(socket))
        {
            if (is_connected())
            {
                update_ip();
                async_handshake();
            }
        }

        Connection(const Connection&) = delete;
        Connection(Connection&&) = delete;

        ~Connection() = default;

        Connection& operator=(const Connection&) = delete;
        Connection& operator=(Connection&&) = delete;

        void disconnect(const std::string& reason = "", bool is_error = false)
        {
            if (is_connected())
            {
                if (!reason.empty())
                    m_on_notification.broadcast(reason, is_error ? Severity::error : Severity::notification);

                m_encrypted_socket.lowest_layer().shutdown(asio::socket_base::shutdown_both);
                m_encrypted_socket.lowest_layer().close();
            }
        }

        [[nodiscard]] bool is_connected() const
        {
            return m_encrypted_socket.lowest_layer().is_open();
        }

        [[nodiscard]] uint32_t get_id() const noexcept
        {
            return m_id;
        }

        [[nodiscard]] std::string_view get_ip() const noexcept
        {
            return m_ip;
        }

        // This should always be called from the Asio thread
        void send_message(Message<Id_type> message)
        {
            m_out_queue.push_back(std::move(message));
            start_writing_message();
        }

        void set_accepted_messages(Accepted_messages_ptr accepted_messages) noexcept
        {
            m_accepted_messages = accepted_messages;
        }

        Delegate<const std::string&, Severity> m_on_notification;
        Delegate<Owned_message<Id_type>> m_on_message;

    private:
        // Updates m_ip member with the current remote ip
        void update_ip()
        {
            if (is_connected())
                m_ip = m_encrypted_socket.lowest_layer().remote_endpoint().address().to_string();
        }

        // Async peforms handshake with the remote and starts waiting for the messages after
        void async_handshake()
        {
            auto handshake_task = [this](asio::error_code error) {
                if (!error)
                {
                    m_has_done_handshake = true;

                    m_on_notification.broadcast(
                        std::format("Succesfull handshake with {}", get_ip()), Severity::notification);

                    // Starts to wait messages
                    async_read_header();

                    // If received any messages to be sent during the handshake, we send them now
                    start_writing_message();
                }
                else
                    disconnect(std::format("Error on handshake because {}", error.message()), true);
            };

            switch (m_handshake_type)
            {
            case Handshake_type::client:
                m_encrypted_socket.async_handshake(asio::ssl::stream_base::client, handshake_task);
                break;
            case Handshake_type::server:
                m_encrypted_socket.async_handshake(asio::ssl::stream_base::server, handshake_task);
                break;
            default:
                break;
            }      
        }

        // Checks if the header is in valid format
        [[nodiscard]] bool validate_header(Message_header<Id_type> header) const
        {
            if (header.m_validation_key != Message_header<Id_type>::CONSTANT_VALIDATION_KEY)
                return false;

            if (header.m_internal_id != Internal_id::not_internal)
                return true; // todo add spesific validation for internal messages

            if (m_accepted_messages != nullptr)
            {
                const auto found_limits = m_accepted_messages->find(header.m_id);

                if (found_limits == m_accepted_messages->end())
                    return false;

                if (header.m_size < found_limits->second.m_min || header.m_size > found_limits->second.m_max)
                    return false;
            }

            return true;
        }

        // Waits for the message header and handles it when received
        void async_read_header()
        {
            m_received_message.m_header.m_validation_key = 0;

            asio::async_read(
                m_encrypted_socket, asio::buffer(&m_received_message.m_header, sizeof(Message_header<Id_type>)),
                [this](asio::error_code error, [[maybe_unused]] size_t size) {
                    if (!error)
                    {
                        if (!validate_header(m_received_message.m_header))
                        {
                            disconnect("Header validation failed", true);
                            return;
                        }

                        // Don't read body if size of message is 0
                        if (m_received_message.m_header.m_size == 0)
                        {
                            on_message_received();
                            async_read_header();
                            return;
                        }

                        m_received_message.resize_body(m_received_message.m_header.m_size);
                        async_read_body();
                    }
                    else
                        disconnect(std::format("Read header failed because {}", error.message()), true);
                });
        }

        // Waits for the message body and handles it when received
        void async_read_body()
        {
            asio::async_read(
                m_encrypted_socket, asio::buffer(m_received_message.m_body.data(), m_received_message.m_body.size()),
                [this](asio::error_code error, [[maybe_unused]] size_t size) {
                    if (!error)
                    {
                        on_message_received();
                        async_read_header();
                    }
                    else
                        disconnect(std::format("Read body failed because {}", error.message()), true);
                });
        }

        // Starts writing message if possible otherwise does nothing
        void start_writing_message()
        {
            if (!m_out_queue.empty() && !m_is_writing_message && m_has_done_handshake)
                async_write_header();
        }

        // Sends the message header over internet in async way
        void async_write_header()
        {
            m_is_writing_message = true;

            asio::async_write(
                m_encrypted_socket, asio::buffer(&m_out_queue.front().m_header, sizeof(Message_header<Id_type>)),
                [this](asio::error_code error, [[maybe_unused]] size_t size) {
                    if (!error)
                    {
                        if (m_out_queue.front().m_header.m_size > 0)
                            async_write_body();
                        else
                        {
                            m_out_queue.pop_front();

                            if (!m_out_queue.empty())
                                async_write_header();
                            else
                                m_is_writing_message = false;
                        }
                    }
                    else
                        disconnect(std::format("Write header failed because {}", error.message()), true);
                });
        }

        // Sends the message body over internet in async way
        void async_write_body()
        {
            asio::async_write(
                m_encrypted_socket, asio::buffer(m_out_queue.front().m_body.data(), m_out_queue.front().m_body.size()),
                [this](asio::error_code error, [[maybe_unused]] size_t size) {
                    if (!error)
                    {
                        m_out_queue.pop_front();

                        if (!m_out_queue.empty())
                            async_write_header();
                        else
                            m_is_writing_message = false;
                    }
                    else
                        disconnect(std::format("Write body failed because {}", error.message()), true);
                });
        }

        // Triggers on_message callback on current reveived_message
        void on_message_received()
        {
            auto owned_message =
                Owned_message<Id_type>(std::move(m_received_message), Client_information(get_id(), get_ip()));
            m_on_message.broadcast(std::move(owned_message));
            m_received_message = Message<Id_type>();
        }

        const uint32_t m_id = 0;
        std::string m_ip = "0.0.0.0";

        Encrypted_socket m_encrypted_socket;
        const Handshake_type m_handshake_type = Handshake_type::client;
        bool m_has_done_handshake = false;

        bool m_is_writing_message = false;
        Message<Id_type> m_received_message;
        Thread_safe_deque<Message<Id_type>> m_out_queue;
        Accepted_messages_ptr m_accepted_messages = nullptr;
    };
} // namespace Net
