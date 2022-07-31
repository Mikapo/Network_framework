#pragma once

#include "../Connection/Client_connection.h"
#include "../Interfaces/Client_connection_interface.h"
#include "../Utility/Net_message.h"
#include "../Utility/Thread_safe_deque.h"
#include "Net_user.h"
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <unordered_set>

namespace Net
{
    constexpr uint32_t Client_id_start = 1000;

    template <Id_concept Id_type>
    class Server : public Net_user<Id_type>
    {
    public:
        using Client_connection = Client_connection<Id_type>;
        using Client_connection_ptr = std::shared_ptr<Client_connection>;

        explicit Server(uint16_t port) : m_acceptor(this->create_acceptor(Protocol::endpoint(Protocol::v4(), port)))
        {
        }

        virtual ~Server()
        {
            stop();
        }

        Server(const Server&) = delete;
        Server(Server&&) = delete;
        Server& operator=(const Server&) = delete;
        Server& operator=(Server&&) = delete;

        bool start()
        {
            try
            {
                async_wait_for_connections();
                this->start_asio_thread();
            }
            catch (const std::exception& exception)
            {
                this->on_notification(std::format("Server start error: {}", exception.what()), Severity::error);
                return false;
            }

            this->on_notification("Server has been started");
            return true;
        }

        void stop()
        {
            this->stop_asio_thread();
            this->on_notification("Server has been stopped");
        }

        void ban_ip(const std::string& ip)
        {
            m_banned_ip.insert(ip);
        }

        void unban_ip(const std::string& ip)
        {
            m_banned_ip.erase(ip);
        }

        void send_message_to_client(Client_connection_ptr client, const Net_message<Id_type>& message)
        {
            if (client && client->is_connected())
                this->async_send_message_to_connection(client.get(), message);
            else
            {
                std::scoped_lock lock(m_connections_mutext);

                notify_client_disconnect(client);
                client.reset();
                m_connections.erase(
                    std::remove(m_connections.begin(), m_connections.end(), client), m_connections.end());
            }
        }

        void send_message_to_all_clients(
            const Net_message<Id_type>& message, Client_connection_ptr ignored_client = nullptr)
        {
            std::scoped_lock lock(m_connections_mutext);

            bool disconnected_clients_exist = false;

            for (auto& client : m_connections)
            {
                if (client && client->is_connected())
                {
                    if (client != ignored_client)
                        this->async_send_message_to_connection(client.get(), message);
                }
                else
                {
                    notify_client_disconnect(client);
                    client.reset();
                    disconnected_clients_exist = true;
                }
            }

            if (disconnected_clients_exist)
                m_connections.erase(
                    std::remove(m_connections.begin(), m_connections.end(), nullptr), m_connections.end());
        }

    protected:
        virtual bool on_client_connect(Client_connection_interface<Id_type> client)
        {
            return true;
        }

        virtual void on_client_disconnect(Client_connection_interface<Id_type> client)
        {
        }

        virtual void on_message(Client_connection_interface<Id_type> client, Net_message<Id_type> message)
        {
        }

    private:
        void handle_received_messages(size_t max_messages) override
        {
            for (size_t i = 0; i < max_messages && !this->is_in_queue_empty(); ++i)
            {
                Owned_message<Id_type> message = this->in_queue_pop_front();
                Client_connection_interface<Id_type> connection_interface(message.m_owner);

                on_message(connection_interface, std::move(message.m_message));
            }
        }

        void handle_new_connection(Protocol::socket socket)
        {
            std::scoped_lock lock(m_connections_mutext);

            const std::string ip = socket.remote_endpoint().address().to_string();

            this->notifications_push_back(std::format("Server new connection: {}", ip));

            if (m_banned_ip.contains(ip))
            {
                this->notifications_push_back(std::format("Client with ip {} is banned", ip));
                return;
            }

            Client_connection_ptr new_connection = this->create_connection<Client_connection>(std::move(socket));

            if (on_client_connect(Client_connection_interface<Id_type>(new_connection)))
            {
                m_connections.push_back(new_connection);
                new_connection->connect_to_client(m_id_counter++);

                this->notifications_push_back(
                    std::format("Client with ip {} was accepted and assigned ip {} to it", ip, m_id_counter - 1));
            }

            else
                this->notifications_push_back(std::format("Connection {} denied", ip));
        }

        void async_wait_for_connections()
        {
            m_acceptor.async_accept([this](asio::error_code error, Protocol::socket socket) {
                if (!error)
                    handle_new_connection(std::move(socket));
                else
                    this->notifications_push_back(
                        std::format("Server connection error: {}", error.message()), Severity::error);

                async_wait_for_connections();
            });
        }

        void notify_client_disconnect(Client_connection_ptr client)
        {
            this->notifications_push_back(
                std::format("Client disconnected ip: {} id: {}", client->get_ip(), client->get_id()));
            on_client_disconnect(Client_connection_interface<Id_type>(client));
        }

        void on_new_accepted_message(Id_type type, Message_limits limits) override
        {
            std::scoped_lock lock(m_connections_mutext);

            for (Client_connection_ptr& connection : m_connections)
                connection->add_accepted_message(type, limits);
        }

        std::deque<Client_connection_ptr> m_connections;
        std::mutex m_connections_mutext;

        Protocol::acceptor m_acceptor;
        uint32_t m_id_counter = Client_id_start;

        std::unordered_set<std::string> m_banned_ip;
    };
} // namespace Net
