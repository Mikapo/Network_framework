#include "User/Ssl/Ssl_server.h"
#include <format>
#include <iostream>
#include <string>
#include <unordered_map>

/**
 *   Different id's for the messages that are send over the internet.
 *   Needs to be same for client and the servers.
 */
enum class Message_id : uint8_t
{
    client_set_name,
    client_message,
    server_message
};

// Names send by the clients
std::unordered_map<uint32_t, std::string> names;

constexpr uint16_t port = 1234;

// the object that handles the server networking
Net::Ssl_server<Message_id> server(port);

// Notifications from the network framework
void server_notification(std::string_view notification, [[maybe_unused]] Net::Severity severity)
{
    std::cout << notification << "\n";
}

// Setups name for the client and sends back accepted string
void on_set_name(uint32_t client_id, Net::Message<Message_id> message)
{
    const std::string name = message.extract_as_string();
    names[client_id] = name;

    const std::string respond = "Name accepted";
    Net::Message<Message_id> net_message;
    net_message.set_id(Message_id::server_message);
    net_message.push_back_string(respond);

    server.send_message_to_client(client_id, net_message);

    std::cout << "Set name " << name << " for client " << client_id << "\n";
}

// Handles received chat messages
void on_chat_message(uint32_t client_id, Net::Message<Message_id> message)
{
    const auto found_name = names.find(client_id);

    const std::string chat_message = message.extract_as_string();
    const std::string formated_message = std::format("[{}] {}", found_name->second, chat_message);

    Net::Message<Message_id> net_message;
    net_message.set_id(Message_id::server_message);
    net_message.push_back_string(formated_message);

    server.send_message_to_all_clients(net_message);

    std::cout << formated_message << "\n";
}

// Event when message is received from the client
void server_on_message(Net::Client_information client, Net::Message<Message_id> message)
{
    switch (message.get_id())
    {
    case Message_id::client_set_name:
        on_set_name(client.m_id, std::move(message));
        break;
    case Message_id::client_message:
        on_chat_message(client.m_id, std::move(message));
        break;
    default:
        server.disconnect_client(client.m_id);
        break;
    }
}

int main()
{
    try
    {
        // Setups accepted messages and callbacks
        server.add_accepted_message(Message_id::client_set_name);
        server.add_accepted_message(Message_id::client_message);

        server.m_on_notification.set_callback(server_notification);
        server.m_on_message.set_callback(server_on_message);

        // setup ssl stuff
        server.set_ssl_certificate_chain_file("server.crt");
        server.set_ssl_private_key_file("server.key");
        server.set_ssl_tmp_dh_file("dh512.pem");

        // Starts the server
        server.start();

        // Main update loop for the server
        while (true)
            server.update(10, true, std::chrono::seconds(30));
    }
    catch (const std::exception& exception)
    {
        std::cout << "Exception: " << exception.what() << "\n";
        std::cout << "Press enter to exit.. \n";
        std::cin.get();
    }
}
