#pragma once

#define ASIO_STANDALONE

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#include <type_traits>
#include "asio.hpp"
#include "asio/buffer.hpp"
#include "asio/socket_base.hpp"

namespace Network
{
	template<typename T>
	concept Enum_concept = std::is_enum_v<T>;

	enum class Severity : uint8_t
	{
		notification,
		error
	};

	constexpr uint64_t VALIDATION_KEY = 9970951313928774000;
}