#include "User/Client.h"
#include <iostream>
#include <string>

enum class Message_id : uint8_t
{
    set_name,
    message,
    server_message
};

bool send_thread_exit_flag = false;
Net::Thread_safe_deque<Net::Message<Message_id>> messages;
Net::Client<Message_id> client;

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

void send_thread()
{
    while (!send_thread_exit_flag)
    {
        std::string message;
        std::getline(std::cin, message);

        if (message.empty())
            continue;

        Net::Message<Message_id> net_message;
        net_message.set_id(Message_id::message);
        net_message.push_back_string(message);

        messages.push_back(net_message);
    }
}

void send_name()
{
    std::cout << "Please enter your username: ";
    std::string username;
    std::getline(std::cin, username);

    Net::Message<Message_id> message;
    message.set_id(Message_id::set_name);
    message.push_back_string(username);

    client.send_message(message);
}

void main_loop()
{
    while (client.is_connected())
    {
        client.update();

        if (!messages.empty())
            client.send_message(messages.pop_front());
    }
}

void start_client()
{
    std::cout << "Write server ip: ";
    std::string ip;
    std::getline(std::cin, ip);
    std::cout << "Write server port: ";
    std::string port;
    std::getline(std::cin, port);

    client.add_accepted_message(Message_id::server_message);
    client.m_on_message.set_function(client_on_message);
    client.connect(ip, port);

    if (client.is_connected())
    {
        std::cout << "Connected succefully \n";
        send_name();

        std::thread thread = std::thread(send_thread);

        main_loop();

        send_thread_exit_flag = true;

        if (thread.joinable())
            thread.join();
    }
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
