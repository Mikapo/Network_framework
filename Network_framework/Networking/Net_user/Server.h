#pragma once

#include "../Connection/Client_connection.h"
#include "../Utility/Net_message.h"
#include "Net_user.h"
#include "../Utility/Thread_safe_deque.h"
#include <cstdint>
#include <memory>
#include <limits>
#include <format>

namespace Net
{
	template<Id_concept Id_type, uint64_t max_message_size = std::numeric_limits<uint64_t>::max()>
	class Server : public Net_user<Id_type, max_message_size>
	{
	public:
		using Client_connection = Client_connection<Id_type, max_message_size>;
		using Client_connection_ptr = std::shared_ptr<Client_connection>;
		using Protocol = asio::ip::tcp;
		
		Server(uint16_t port)
			: m_acceptor(this->m_asio_context, Protocol::endpoint(Protocol::v4(), port))
		{

		}

		virtual ~Server()
		{
			stop();
		}

		bool start()
		{
			try 
			{
				async_wait_for_connections();
				this->m_thread_handle = std::thread([this] { this->m_asio_context.run(); });
			}
			catch (std::exception exception)
			{
				this->on_notification(std::format("Server start error: {}", exception.what()), Severity::error);
				return false;
			}

			this->on_notification("Server has been started");
			return true;
		}

		void stop()
		{
			this->m_asio_context.stop();

			if (this->m_thread_handle.joinable())
				this->m_thread_handle.join();
			this->on_notification("Server has been stopped");
		}

		void async_wait_for_connections()
		{
			m_acceptor.async_accept(
				[this](asio::error_code error, Protocol::socket socket)
				{
					if (!error)
					{
						this->on_notification(std::format("Server new connection: {}", socket.remote_endpoint().address().to_string()));

						std::shared_ptr new_connection = std::make_shared<Client_connection>(this->m_asio_context, std::move(socket));

						if (on_client_connect(new_connection))
						{
							m_connections.push_back(new_connection);
							m_connections.back()->connect_to_client(m_id_counter++);

							m_connections.back()->set_on_message_received_callback(
								[this](const Net_message<Id_type>& message, Client_connection_ptr connection)
								{
									on_message_received(message, connection);
								});

							this->on_notification("Server connection approved");
						}
						else
						{
							this->on_notification("Server connection denied");
						}

					}
					else
						this->on_notification(std::format("Server connection error: {}", error.message()), Severity::error);

					async_wait_for_connections();
				});
		}

		void send_message_to_client(Client_connection_ptr client, const Net_message<Id_type>& message)
		{
			if (client && client->is_connected())
				client->send_message(message);
			else
			{
				this->on_notification("Client disconnected");
				on_client_disconnect(client);
				client.reset();
				m_connections.erase(std::remove(m_connections.begin(), m_connections.end(), client), m_connections.end());
			}
		}

		void send_message_to_all_clients(const Net_message<Id_type>& message, Client_connection_ptr ignored_client = nullptr)
		{
			bool disconnected_clients_exist = false;

			for (auto& client : m_connections)
			{
				if (client && client->is_connected())
				{
					if (client != ignored_client)
						client->send_message(message);
				}
				else
				{
					this->on_notification("Client disconnected");
					on_client_disconnect(client);
					client.reset();
					disconnected_clients_exist = true;
				}

				if (disconnected_clients_exist)
					m_connections.erase(std::remove(m_connections.begin(), m_connections.end(), nullptr), m_connections.end());
				
			}
		}

		void handle_received_messages(size_t max_messages = std::numeric_limits<size_t>::max())
		{
			for (size_t i = 0; i < max_messages && !this->m_in_queue.empty(); ++i)
			{
				auto message = this->m_in_queue.pop_front();
				on_message(message.m_owner, message.m_message);
			}
		}

	protected:
		virtual bool on_client_connect(Client_connection_ptr client)
		{
			return true;
		}

		virtual void on_client_disconnect(Client_connection_ptr client)
		{

		}

		virtual void on_message(Client_connection_ptr client, Net_message<Id_type>& message)
		{

		}

	private:
		void on_message_received(const Net_message<Id_type>& message, Client_connection_ptr connection)
		{
			Owned_message<Id_type, max_message_size> owned_message = { .m_owner = connection, .m_message = message };
			this->m_in_queue.push_back(std::move(owned_message));
		}

		std::deque<Client_connection_ptr> m_connections;
		Protocol::acceptor m_acceptor;
		uint32_t m_id_counter = 10000;
	};
}