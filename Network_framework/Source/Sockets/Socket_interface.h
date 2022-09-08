#pragma once

#include "../Events/Delegate.h"
#include "../Utility/Common.h"

namespace Net
{
    enum class Handshake_type : uint8_t
    {
        client,
        server
    };

    class Socket_interface
    {
    public:
        Socket_interface() = default;
        virtual ~Socket_interface() = default;

        Socket_interface(const Socket_interface&) = delete;
        Socket_interface(Socket_interface&&) = default;

        Socket_interface& operator=(const Socket_interface&) = delete;
        Socket_interface& operator=(Socket_interface&&) = default;

        virtual void async_handshake(Handshake_type type) = 0;

        virtual void async_read_header(void* buffer, size_t size) = 0;
        virtual void async_read_body(void* buffer, size_t size) = 0;
        virtual void async_write_header(const void* buffer, size_t size) = 0;
        virtual void async_write_body(const void* buffer, size_t size) = 0;

        [[nodiscard]] virtual bool is_open() const = 0;
        [[nodiscard]] virtual std::string get_ip() const = 0;
        virtual void disconnect() = 0;

        Delegate<asio::error_code> m_handshake_finished;
        Delegate<asio::error_code, size_t> m_read_header_finished;
        Delegate<asio::error_code, size_t> m_read_body_finished;
        Delegate<asio::error_code, size_t> m_write_header_finished;
        Delegate<asio::error_code, size_t> m_write_body_finished;

    private:
    };
} // namespace Net
