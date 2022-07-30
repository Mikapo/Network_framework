#include "Net_framework.h"
#include <iostream>

#include <unordered_map>

enum class Message_id : uint8_t
{
    set_name = 0,
    message = 1,
    server_message = 3
};

class Chat_server : public Net::Server_interface<Message_id>
{
public:
    Chat_server(uint16_t port) : Server_interface<Message_id>(port)
    {
        add_accepted_message(Message_id::set_name, 0, 10);
        add_accepted_message(Message_id::message);
    }

private:
    void on_message(Net::Client_connection_interface<Message_id> client, Net::Net_message<Message_id> message) override
    {
        switch (message.get_id())
        {
        case Message_id::set_name: {
            const std::string name = message.extract_as_string();
            m_names[client.get_id()] = name;

            std::cout << "Set name " << name << " for client " << client.get_id() << "\n";

            const std::string output = "Name accepted you can send messages now";

            Net::Net_message<Message_id> net_message;
            net_message.set_id(Message_id::server_message);
            net_message.push_back_from_container(output.begin(), output.end());
            send_message_to_client(client, net_message);

            const std::string join_message = std::format("{} joined the chat", name);

            Net::Net_message<Message_id> join_net_message;
            join_net_message.set_id(Message_id::server_message);
            join_net_message.push_back_from_container(join_message.begin(), join_message.end());
            send_message_to_all_clients(join_net_message, client);

            break;
        }
        case Message_id::message: {
            auto found_name = m_names.find(client.get_id());

            if (found_name == m_names.end())
            {
                client.disconnect();
                break;
            }

            const std::string message_string = message.extract_as_string();
            const std::string output_string = std::format("[{}]: {}", found_name->second, message_string);

            Net::Net_message<Message_id> net_message;
            net_message.set_id(Message_id::server_message);
            net_message.push_back_from_container(output_string.begin(), output_string.end());

            std::cout << output_string << "\n";

            send_message_to_all_clients(net_message, client);
            break;
        }
        default:
            break;
        }
    }

    void on_notification(std::string_view notification, Net::Severity severity = Net::Severity::notification) override
    {
        std::cout << notification << "\n";
    }

    std::unordered_map<uint32_t, std::string> m_names;
};

void run_server()
{
    Chat_server server(1234);
    server.start();

    while (true)
        server.handle_received_messages(10, true);
}

int main()
{
    try
    {
        run_server();
    }
    catch (const std::exception& exception)
    {
        std::cout << "Exception: " << exception.what() << "\n";
    }

    std::cout << "Now exiting server \n";
}
