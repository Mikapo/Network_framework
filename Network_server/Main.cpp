#include "Net_framework.h"
#include <iostream>

#include <unordered_map>

enum class Message_id : uint8_t
{
    set_name,
    message,
    server_message
};

class Chat_server : public Net::Server_interface<Message_id>
{
public:
    explicit Chat_server(uint16_t port) : Server_interface<Message_id>(port)
    {
        add_accepted_message(Message_id::set_name);
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
            net_message.push_back_string(output);
            send_message_to_client(client, net_message);

            const std::string join_message = std::format("{} joined the chat", name);

            Net::Net_message<Message_id> join_net_message;
            join_net_message.set_id(Message_id::server_message);
            join_net_message.push_back_string(join_message);
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
            net_message.push_back_string(output_string);

            std::cout << output_string << "\n";

            send_message_to_all_clients(net_message, client);
            break;
        }
        default:
            break;
        }
    }

    void on_notification(std::string_view notification, Net::Severity severity) override
    {
        std::cout << notification << "\n";
    }

    std::unordered_map<uint32_t, std::string> m_names;
};

void run_server()
{
    constexpr uint16_t port = 1234;
    Chat_server server(port);
    server.start();

    constexpr size_t max_messages = 10;

    while (true)
        server.update(max_messages, true, std::chrono::seconds(60));
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
