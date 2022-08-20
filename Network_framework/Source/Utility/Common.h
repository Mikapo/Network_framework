#pragma once

#define ASIO_STANDALONE
#define ASIO_NO_DEPRECATED

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#include "asio.hpp"
#include "asio/buffer.hpp"
#include "asio/socket_base.hpp"
#include <type_traits>

namespace Net
{
    using Protocol = asio::ip::tcp;

    template <typename T>
    concept Id_concept = std::is_enum_v<T>;

    enum class Severity : uint8_t
    {
        notification,
        error
    };
} // namespace Net
