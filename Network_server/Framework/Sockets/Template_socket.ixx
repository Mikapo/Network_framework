module;
#include "../Utility/Asio_includes.h"
#include <cstdint>
#include <string>

export module Network_framework:Template_socket;
import :Socket_interface;

export namespace Net
{
    template <typename Asio_socket>
    class Template_socket : public Socket_interface
    {
    public:
        Template_socket(Asio_socket socket) : m_socket(std::move(socket))
        {
        }

        void async_handshake(Handshake_type type) override
        {
            // Only do handshake is we have ssl socket
            if constexpr (std::is_same_v<Asio_socket, Ssl_socket>)
            {
                switch (type)
                {
                case Handshake_type::client:
                    m_socket.async_handshake(asio::ssl::stream_base::client, [this](asio::error_code error) {
                        m_handshake_finished.broadcast(error);
                    });
                    break;

                case Handshake_type::server:
                    m_socket.async_handshake(asio::ssl::stream_base::server, [this](asio::error_code error) {
                        m_handshake_finished.broadcast(error);
                    });
                    break;
                }
            }
            else
                m_handshake_finished.broadcast(asio::error_code());
        }

        void async_read_header(void* buffer, size_t size) override
        {
            asio::async_read(m_socket, asio::buffer(buffer, size), [this](asio::error_code error, size_t bytes) {
                m_read_header_finished.broadcast(error, bytes);
            });
        };

        void async_read_body(void* buffer, size_t size) override
        {
            asio::async_read(m_socket, asio::buffer(buffer, size), [this](asio::error_code error, size_t bytes) {
                m_read_body_finished.broadcast(error, bytes);
            });
        }

        void async_write_header(const void* buffer, size_t size) override
        {
            asio::async_write(m_socket, asio::buffer(buffer, size), [this](asio::error_code error, size_t bytes) {
                m_write_header_finished.broadcast(error, bytes);
            });
        }

        void async_write_body(const void* buffer, size_t size) override
        {
            asio::async_write(m_socket, asio::buffer(buffer, size), [this](asio::error_code error, size_t bytes) {
                m_write_body_finished.broadcast(error, bytes);
            });
        }

        void disconnect() override
        {
            if (is_open())
            {
                m_socket.lowest_layer().shutdown(asio::socket_base::shutdown_both);
                m_socket.lowest_layer().close();
            }
        }

        bool is_open() const noexcept override
        {
            return m_socket.lowest_layer().is_open();
        }

        std::string get_ip() const override
        {
            if (is_open())
                return m_socket.lowest_layer().remote_endpoint().address().to_string();
            else
                return "0.0.0.0";
        }

    private:
        Asio_socket m_socket;
    };
} // namespace Net
