#pragma once

#include "../Net_user/Client.h"

namespace Net
{
	template<Id_concept Id_type, uint64_t max_message_size = std::numeric_limits<uint64_t>::max()>
	class Client_interface : private Client<Id_type, max_message_size>
	{
	public:
		using Underlying = Client<Id_type, max_message_size>;

		bool connect(std::string_view host, uint16_t port)
		{
			return this->Underlying::connect(host, port);
		}

		void disconnect()
		{
			this->Underlying::disconnect();
		}

		bool is_connected() const noexcept
		{
			return this->Underlying::is_connected();
		}

		void send_message(const Net_message<Id_type>& message)
		{
			this->Underlying::send_message(message);
		}

		void handle_received_messages(size_t max_messages = std::numeric_limits<size_t>::max())
		{
			this->Underlying::handle_received_messages(max_messages);
		}

		void on_message(Net_message<Id_type>& message) = 0;
	};
}
