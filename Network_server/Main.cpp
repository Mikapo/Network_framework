#include <iostream>
#include "Net_framework.h"

#include <unordered_map>

enum class Message_id : uint8_t
{
	set_name,
	message,
	server_message
};

class Chat_server : public Network::Server<Message_id, 12>
{
public:
	Chat_server(uint16_t port)
		: Server<Message_id, 12>(port) {}

private:

	void on_message(std::shared_ptr<Network::Client_connection<Message_id, 12>> client, Network::Net_message<Message_id>& message) override
	{
		switch (message.m_header.m_id)
		{
		case Message_id::set_name:
		{
			const std::string name = get_string_from_message(message);
			m_names[client->get_id()] = name;
			
			Network::Net_message<Message_id> net_message;
			net_message.m_header.m_id = Message_id::server_message;
			
			std::string output = "Name accepted you can now send messages";

			for (size_t i = 0; i < output.size(); ++i)
				net_message << output.at(i);

			send_message_to_client(client, net_message);

			break;
		}
		case Message_id::message:
		{
			auto found_name = m_names.find(client->get_id());

			if (found_name == m_names.end())
			{
				client->disconnect();
				break;
			}

			const std::string message_string = get_string_from_message(message);
			const std::string output_string = std::format("[{}]: {}", found_name->second, message_string);
			
			Network::Net_message<Message_id> net_message;
			net_message.m_header.m_id = Message_id::server_message;

			for (size_t i = 0; i < output_string.size(); ++i)
				net_message << output_string.at(i);

			std::cout << output_string << "\n";

			send_message_to_all_clients(net_message);
			break;
		}
		default:
			break;
		}
	} 

	void on_notification(std::string_view notification, Network::Severity severity = Network::Severity::notification) override
	{
		std::cout << notification << "\n";
	}

	std::string get_string_from_message(const Network::Net_message<Message_id>& message)
	{
		std::string output;
		for (size_t i = 0; i < message.m_body.size(); ++i)
		{
			if(message.m_body.at(i) != '\n')
				output += message.m_body.at(i);
		}
		return output;
	}

	std::unordered_map<uint32_t, std::string> m_names;
};

int main()
{
	Chat_server server(1234);
	server.start();

	while (true)
	{
		server.handle_received_messages();
	}
}