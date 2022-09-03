module;
#include <limits>
#include "Asio_includes.h"

export module Network_framework:Common;

// This file has code that can be used anywhere in the framework;
export namespace Net
{
    constexpr size_t SIZE_T_MAX = std::numeric_limits<size_t>::max();

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
