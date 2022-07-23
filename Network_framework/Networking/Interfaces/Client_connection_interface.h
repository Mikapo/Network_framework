#pragma once

#include "../Connection/Client_connection.h"

namespace Net
{
	template<Id_concept Id_type, uint64_t max_message_size>
	class Server_interface;

	template<Id_concept Id_type, uint64_t max_message_size = std::numeric_limits<uint64_t>::max()>
	class Client_connection_interface
	{
	public:
		friend Server_interface<Id_type, max_message_size>;
		using Client_connection_ptr = std::shared_ptr<Client_connection<Id_type, max_message_size>>;

		Client_connection_interface(Client_connection_ptr ptr)
			: m_client_connection(ptr) {}

		void disconnect()
		{
			if (m_client_connection)
				m_client_connection->disconnect();
		}

		bool is_connected() const 
		{
			if (m_client_connection)
				return m_client_connection->is_connected();
			else
				return false;
		}

		uint32_t get_id() const noexcept
		{
			if (m_client_connection)
				return m_client_connection->get_id();
			else
				return 0;
		}

	private:
		Client_connection_ptr get_underlying() const noexcept
		{
			return m_client_connection;
		}

		Client_connection_ptr m_client_connection;
	};
}