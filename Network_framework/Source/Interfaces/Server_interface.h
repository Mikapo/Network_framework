#pragma once

#include "../Net_user/Server.h"
#include "Client_connection_interface.h"

namespace Net
{
    template <Id_concept Id_type>
    class Server_interface : Server<Id_type>
    {
    public:
        using Underlying = Server<Id_type>;

        Server_interface(uint16_t port) : Underlying(port)
        {
        }

        bool start()
        {
            return this->Underlying::start();
        }

        void stop()
        {
            this->Underlying::stop();
        }

        void send_message_to_client(Client_connection_interface<Id_type> client, const Net_message<Id_type>& message)
        {
            this->Underlying::send_message_to_client(client.get_underlying(), message);
        }

        void send_message_to_all_clients(
            const Net_message<Id_type>& message,
            Client_connection_interface<Id_type> ignored_client = Client_connection_interface<Id_type>(nullptr))
        {
            this->Underlying::send_message_to_all_clients(message, ignored_client.get_underlying());
        }

        void handle_received_messages(size_t max_messages = std::numeric_limits<size_t>::max(), bool wait = false)
        {
            this->Underlying::handle_received_messages(max_messages, wait);
        }

        void add_accepted_message(Id_type type, uint32_t min = 0, uint32_t max = std::numeric_limits<uint32_t>::max())
        {
            this->Underlying::add_accepted_message(type, min, max);
        }

        bool on_client_connect(Client_connection_interface<Id_type> client) override
        {
            return client;
        }

        void on_client_disconnect(Client_connection_interface<Id_type> client) override
        {
        }

        void on_message(Client_connection_interface<Id_type> client, Net_message<Id_type> message) override
        {
        }

        void on_notification(std::string_view notification, Severity severity = Severity::notification) override
        {
        }
    };
} // namespace Net
