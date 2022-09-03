module;
#include <format>
#include <unordered_map>

export module Network_framework:Connection;
import :Owned_message;
import :Socket_interface;
import :Common;
import :Delegate;
import :Thread_safe_deque; 


export namespace Net
{
    struct Message_limits
    {
        uint32_t m_min = 0, m_max = 0;
    };

    // Class that repesents remote net connection
    template <Id_concept Id_type>
    class Connection
    {
    public:
        using Accepted_messages_ptr = std::shared_ptr<const std::unordered_map<Id_type, Message_limits>>;
        using End_points = Protocol::resolver::results_type;

        Connection(std::unique_ptr<Socket_interface> socket, uint32_t connection_id)
            : m_id(connection_id), m_socket(std::move(socket))
        {
        }

        Connection(const Connection&) = delete;
        Connection(Connection&&) = delete;

        ~Connection() = default;

        Connection& operator=(const Connection&) = delete;
        Connection& operator=(Connection&&) = delete;

        // Starts the handshake and listening to messages
        void start(Handshake_type handshake_type)
        {
            if (is_connected())
            {
                setup_callbacks_on_socket();
                update_ip();
                m_socket->async_handshake(handshake_type);
            }
        }

        void disconnect(const std::string& reason = "", bool is_error = false)
        {
            if (is_connected())
            {
                if (!reason.empty())
                    m_on_notification.broadcast(reason, is_error ? Severity::error : Severity::notification);

                m_socket->disconnect();
            }
        }

        [[nodiscard]] bool is_connected() const
        {
            return m_socket->is_open();
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
        void setup_callbacks_on_socket() noexcept
        {
            m_socket->m_handshake_finished.set_callback(
                [this](asio::error_code error) { async_handshake_finished(error); });

            m_socket->m_read_header_finished.set_callback(
                [this](asio::error_code error, size_t bytes) { async_read_header_finished(error, bytes); });

            m_socket->m_read_body_finished.set_callback(
                [this](asio::error_code error, size_t bytes) { async_read_body_finished(error, bytes); });

            m_socket->m_write_header_finished.set_callback(
                [this](asio::error_code error, size_t bytes) { async_write_header_finished(error, bytes); });

            m_socket->m_write_body_finished.set_callback(
                [this](asio::error_code error, size_t bytes) { async_write_body_finished(error, bytes); });
        }

        // Updates m_ip member with the current remote ip
        void update_ip()
        {
            if (is_connected())
                m_ip = m_socket->get_ip();
        }

        // Events when handshake is finished
        void async_handshake_finished(asio::error_code error)
        {
            if (!error)
            {
                m_has_done_handshake = true;

                m_on_notification.broadcast(
                    std::format("Succesfull handshake with {}", get_ip()), Severity::notification);

                // Starts to wait messages
                m_socket->async_read_header(&m_received_message.m_header, sizeof(Message_header<Id_type>));

                // If received any messages to be sent during the handshake, we send them now
                start_writing_message();
            }
            else
                disconnect(std::format("Error on handshake because {}", error.message()), true);
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

        // Event when read header is finished
        void async_read_header_finished(asio::error_code error, [[maybe_unused]] size_t bytes)
        {
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
                    m_socket->async_read_header(&m_received_message.m_header, sizeof(Message_header<Id_type>));
                    return;
                }

                m_received_message.resize_body(m_received_message.m_header.m_size);
                m_socket->async_read_body(m_received_message.m_body.data(), m_received_message.m_body.size());
            }
            else
                disconnect(std::format("Read header failed because {}", error.message()), true);
        }

        // Event when read body is finished
        void async_read_body_finished(asio::error_code error, [[maybe_unused]] size_t bytes)
        {
            if (!error)
            {
                on_message_received();
                m_socket->async_read_header(&m_received_message.m_header, sizeof(Message_header<Id_type>));
            }
            else
                disconnect(std::format("Read body failed because {}", error.message()), true);
        }

        // Starts writing message if possible otherwise does nothing
        void start_writing_message()
        {
            if (!m_out_queue.empty() && !m_is_writing_message && m_has_done_handshake)
            {
                m_is_writing_message = true;
                m_socket->async_write_header(&m_out_queue.front().m_header, sizeof(Message_header<Id_type>));
            }
        }

        // Event when write header is finished
        void async_write_header_finished(asio::error_code error, [[maybe_unused]] size_t bytes)
        {
            if (!error)
            {
                if (m_out_queue.front().m_header.m_size > 0)
                    m_socket->async_write_body(m_out_queue.front().m_body.data(), m_out_queue.front().m_body.size());

                else
                {
                    m_out_queue.pop_front();

                    if (!m_out_queue.empty())
                        m_socket->async_write_header(&m_out_queue.front().m_header, sizeof(Message_header<Id_type>));
                    else
                        m_is_writing_message = false;
                }
            }
            else
                disconnect(std::format("Write header failed because {}", error.message()), true);
        }

        // Event when writing to body is finished
        void async_write_body_finished(asio::error_code error, [[maybe_unused]] size_t bytes)
        {
            if (!error)
            {
                m_out_queue.pop_front();

                if (!m_out_queue.empty())
                    m_socket->async_write_header(&m_out_queue.front().m_header, sizeof(Message_header<Id_type>));
                else
                    m_is_writing_message = false;
            }
            else
                disconnect(std::format("Write body failed because {}", error.message()), true);
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

        std::unique_ptr<Socket_interface> m_socket;
        bool m_has_done_handshake = false;

        bool m_is_writing_message = false;
        Message<Id_type> m_received_message;
        Thread_safe_deque<Message<Id_type>> m_out_queue;
        Accepted_messages_ptr m_accepted_messages = nullptr;
    };
} // namespace Net

