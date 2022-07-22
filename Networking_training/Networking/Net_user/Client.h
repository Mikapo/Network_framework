#pragma once

#include "../Utility/Thread_safe_deque.h"
#include "../Connection/Server_connection.h"
#include "Net_user.h"
#include <memory>
#include <cstdint>

namespace Network
{
	template<Enum_concept Id_enum_type, uint64_t max_message_size = std::numeric_limits<uint64_t>::max()>
	class Client : public Net_user<Id_enum_type, max_message_size>
	{
	public:
		using Server_connection = Server_connection<Id_enum_type, max_message_size>;
		using Server_connection_ptr = std::unique_ptr<Server_connection>;
		using Protocol = asio::ip::tcp;

		Client()
			: m_socket(this->m_asio_context)
		{

		}

		virtual ~Client()
		{
			disconnect();
		}

		bool connect(std::string_view host, uint16_t port)
		{
			try
			{
				Protocol::resolver resolver(this->m_asio_context);
				auto endpoints= resolver.resolve(host, std::to_string(port));

				m_connection = std::make_unique<Server_connection>(
					this->m_asio_context,
					Protocol::socket(this->m_asio_context));

				m_connection->set_on_message_received_callback(
					[this](const Net_message<Id_enum_type>& message)
					{
						on_message_received(message);
					});


				m_connection->connect_to_server(endpoints);
				this->m_thread_handle = std::thread([this] { this->m_asio_context.run(); });

			}
			catch (std::exception exception)
			{
				return false;
			}

			return true;
		}

		void disconnect()
		{
			if (is_connected())
				m_connection->disconnect();

			this->m_asio_context.stop();

			if (this->m_thread_handle.joinable())
				this->m_thread_handle.join();

			m_connection.reset();
		}

		bool is_connected() const noexcept
		{
			if (m_connection)
				return m_connection->is_connected();
			else
				return false;
		}

		void send_message(const Net_message<Id_enum_type>& message)
		{
			if (is_connected())
				m_connection->send_message(message);
		}

		void handle_received_messages(size_t max_messages = std::numeric_limits<size_t>::max())
		{
			for (size_t i = 0; i < max_messages && !this->m_in_queue.empty(); ++i)
			{
				auto message = this->m_in_queue.pop_front();
				on_message(message.m_message);
			}
		}

	protected:
		virtual void on_message(Net_message<Id_enum_type>& message)
		{

		}

	private:
		void on_message_received(const Net_message<Id_enum_type>& message)
		{
			Owned_message<Id_enum_type, max_message_size> owned_message = { .m_owner = nullptr, .m_message = message };
			this->m_in_queue.push_back(std::move(owned_message));
		}

		Protocol::socket m_socket;
		Server_connection_ptr m_connection;
	};
}
