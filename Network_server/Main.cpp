#include "Net_user/Server.h"
#include <format>
#include <iostream>
#include <string>
#include <unordered_map>

enum class Message_id : uint8_t
{
    set_name,
    message,
    server_message
};

std::unordered_map<uint32_t, std::string> names;

constexpr uint16_t port = 1234;
Net::Server<Message_id> server(port);

void server_notification(std::string_view notification, Net::Severity severity)
{
    std::cout << notification << "\n";
}

void on_set_name(uint32_t client_id, Net::Net_message<Message_id> message)
{
    const std::string name = message.extract_as_string();
    names[client_id] = name;

    const std::string respond = "Name accepted";
    Net::Net_message<Message_id> net_message;
    net_message.set_id(Message_id::server_message);
    net_message.push_back_string(respond);

    server.send_message_to_client(client_id, net_message);
}

void on_chat_message(uint32_t client_id, Net::Net_message<Message_id> message)
{
    const auto found_name = names.find(client_id);

    const std::string chat_message = message.extract_as_string();
    const std::string formated_message = std::format("[{}] {}", found_name->second, chat_message);

    Net::Net_message<Message_id> net_message;
    net_message.set_id(Message_id::server_message);
    net_message.push_back_string(formated_message);

    server.send_message_to_all_clients(net_message);
}

void server_on_message(Net::Client_information client, Net::Net_message<Message_id> message)
{
    switch (message.get_id())
    {
    case Message_id::set_name:
        on_set_name(client.m_id, std::move(message));
        break;
    case Message_id::message:
        on_chat_message(client.m_id, std::move(message));
        break;
    default:
        break;
    }
}

int main()
{
    server.add_accepted_message(Message_id::set_name);
    server.add_accepted_message(Message_id::message);

    server.m_on_notification.set_function(server_notification);
    server.m_on_message.set_function(server_on_message);

    server.start();

    while (true)
        server.update(10, true, std::chrono::seconds(30));
}
