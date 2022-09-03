#pragma once

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

