#pragma once

// This file has code that can be used anywhere in the framework

#define ASIO_STANDALONE
#define ASIO_NO_DEPRECATED

// Specifying Windows version for Asio
#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#include "asio.hpp"
#include "asio/buffer.hpp"
#include "asio/socket_base.hpp"
#include "asio/ssl.hpp"
#include <limits>

namespace Net
{
    static constexpr size_t SIZE_T_MAX = std::numeric_limits<size_t>::max();

    // The Asio types that we currently use in this framework
    using Protocol = asio::ip::tcp;
    using Ssl_socket = asio::ssl::stream<Protocol::socket>;
 

    // Notification severities
    enum class Severity : uint8_t
    {
        notification,
        error
    };
} // namespace Net
