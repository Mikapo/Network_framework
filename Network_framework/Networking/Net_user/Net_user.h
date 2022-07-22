#pragma once

#include "../Utility/Net_common.h"
#include "../Utility/Thread_safe_deque.h"
#include <thread>

namespace Network
{
	template<Enum_concept Id_enum_type, uint64_t max_message_size = std::numeric_limits<uint64_t>::max()>
	class Net_user
	{
	public:
		using Owned_message = Owned_message<Id_enum_type, max_message_size>;

	protected:
		virtual void on_notification(std::string_view notification, Severity severity = Severity::notification)
		{

		}

		Thread_safe_deque<Owned_message> m_in_queue;
		std::thread m_thread_handle;
		asio::io_context m_asio_context;
	};
};
