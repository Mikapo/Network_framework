#include "Net_framework.h"
#include <iostream>
#include <string>

enum class Message_id : uint8_t
{
	set_name,
	message,
	server_message
};

class Chat_client : public Network::Client<Message_id>
{
private:
	void on_message(Network::Net_message<Message_id>& message) override
	{
		switch (message.m_header.m_id)
		{
		case Message_id::server_message:
		{
			const std::string string = get_string_from_message(message);
			std::cout << string << "\n";
			std::cout.flush();
		}

		default:
			break;
		}

	}

	std::string get_string_from_message(const Network::Net_message<Message_id>& message)
	{
		std::string output;
		for (size_t i = 0; i < message.m_body.size(); ++i)
			output += message.m_body.at(i);
		return output;
	}
};


bool send_thread_exit_flag = false;
Network::Thread_safe_deque<Network::Net_message<Message_id>> messages;

void send_thread()
{
	while (!send_thread_exit_flag)
	{
		std::string message;
		std::getline(std::cin, message);

		Network::Net_message<Message_id> net_message;
		net_message.m_header.m_id = Message_id::message;

		for (size_t i = 0; i < message.size(); ++i)
			net_message << message.at(i);

		messages.push_back(net_message);
	}
}

void send_name(Chat_client& client)
{
	std::cout << "Please enter your username: ";
	std::string username;
	std::getline(std::cin, username);

	Network::Net_message<Message_id> message;
	message.m_header.m_id = Message_id::set_name;

	for (size_t i = 0; i < username.size(); ++i)
		message << username.at(i);

	client.send_message(message);
}

void main_loop(Chat_client& client)
{
	while (client.is_connected())
	{
		client.handle_received_messages();

		if (!messages.empty())
			client.send_message(messages.pop_front());
	}
}

int main()
{
	std::cout << "Write server ip: ";
	std::string ip;
	std::getline(std::cin, ip);
	std::cout << "Write server port: ";
	std::string port;
	std::getline(std::cin, port);

	std::stringstream ss;
	ss << port;
	uint16_t port_numper;
	ss >> port_numper;

	Chat_client client;
	client.connect(ip, port_numper);
	
	if (client.is_connected())
	{
		std::cout << "Connected succefully \n";
		send_name(client);

		std::thread thread = std::thread(send_thread);

		main_loop(client);

		send_thread_exit_flag = true;

		if (thread.joinable())
			thread.join();
	}
	else
	{
		std::cout << "failed to connect \n";
		return 1;
	}

	std::cout << "Server disconnected \n";
}



