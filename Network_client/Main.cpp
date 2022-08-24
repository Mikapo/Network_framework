#include "User/Client.h"
#include <iostream>
#include <string>

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

bool send_thread_exit_flag = false;

// Messages to be send
Net::Thread_safe_deque<Net::Message<Message_id>> messages;

// Client object that handles the client networking
Net::Client<Message_id> client;

// Thread for reading inputs so it will not block the client
std::thread thread;

// Event when framework gives notification
void client_notification(std::string_view notification, Net::Severity severity)
{
    std::cout << notification << "\n";
}

// Event when received the message
void client_on_message(Net::Message<Message_id> message)
{
    switch (message.get_id())
    {
    case Message_id::server_message: {
        const std::string chat_message = message.extract_as_string();
        std::cout << chat_message << "\n";
        break;
    }
    default:
        client.disconnect();
        break;
    }
}

// Sends user selected name to the server
void send_name()
{
    std::cout << "Please enter your username: ";
    std::string username;
    std::getline(std::cin, username);

    Net::Message<Message_id> message;
    message.set_id(Message_id::client_set_name);
    message.push_back_string(username);

    client.send_message(message);
}

// Seperate thread to read input from user for messages
void send_thread()
{
    send_name();

    while (!send_thread_exit_flag)
    {
        std::string message;
        std::getline(std::cin, message);

        if (message.empty())
            continue;

        Net::Message<Message_id> net_message;
        net_message.set_id(Message_id::client_message);
        net_message.push_back_string(message);

        messages.push_back(net_message);
    }
}

// Event_when connected to server
void on_connected()
{
    // Starts new thread for reading inputs from the user
    thread = std::thread(send_thread);
}

// Main logic loop for client
void main_loop()
{
    while (true)
    {
        client.update();

        // Sends message to server
        if (!messages.empty())
            client.send_message(messages.pop_front());
    }
}

void start_client()
{
    // Allows user to write the server ip and port
    std::cout << "Write server ip: ";
    std::string server_ip;
    std::getline(std::cin, server_ip);
    std::cout << "Write server port: ";
    std::string server_port;
    std::getline(std::cin, server_port);

    // Setups client accepted messages and callbacks
    client.add_accepted_message(Message_id::server_message);
    client.m_on_message.set_callback(client_on_message);
    client.m_on_notification.set_callback(client_notification);
    client.m_on_connected.set_callback(on_connected);

    // Setup ssl stuff
    client.set_ssl_verify_file("server.crt");

    // Attempts to connect to the server
    client.connect(server_ip, server_port);

    main_loop();

    // Stops the send thread
    send_thread_exit_flag = true;

    if (thread.joinable())
        thread.join();

    else
    {
        std::cout << "failed to connect \n";
        return;
    }

    std::cout << "Lost connection to server \n";
}

int main()
{
    try
    {
        start_client();
    }
    catch (const std::exception& exception)
    {
        std::cout << "Exception: " << exception.what() << "\n";
    }

    std::cout << "Press enter to exit... \n";
    std::cin.get();
}
