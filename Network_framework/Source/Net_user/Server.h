#pragma once

#include "../Connection/Client_connection.h"
#include "../Interfaces/Client_connection_interface.h"
#include "../Utility/Net_message.h"
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
                this->notifications_push_back(std::format("Server start error: {}", exception.what()), Severity::error);
                return false;
            }

            this->notifications_push_back("Server has been started");
            return true;
        }

        void stop()
        {
            this->stop_asio_thread();
            this->notifications_push_back("Server has been stopped");
        }

        void update(size_t max_handled_items, bool wait, std::optional<std::chrono::seconds> check_connections_interval)
            override
        {
            Net_user<Id_type>::update(max_handled_items, wait, check_connections_interval);

            handle_received_messages(max_handled_items);
            handle_new_connections(max_handled_items);
        }

        void ban_ip(const std::string& new_banned_ip)
        {
            m_banned_ip.insert(new_banned_ip);
        }

        void unban_ip(const std::string& new_unbanned_ip)
        {
            m_banned_ip.erase(new_unbanned_ip);
        }

        void send_message_to_client(Client_connection_ptr client, const Net_message<Id_type>& message)
        {
            if (client && client->is_connected())
                this->async_send_message_to_connection(client.get(), message);
            else if (client)
                remove_connection(m_connections.find(client));
        }

        void send_message_to_all_clients(
            const Net_message<Id_type>& message, Client_connection_ptr ignored_client = nullptr)
        {
            auto connections_iterator = m_connections.begin();
            while (connections_iterator != m_connections.end())
            {
                Client_connection_ptr client = *connections_iterator;

                if (client && client->is_connected())
                {
                    if (client != ignored_client)
                        this->async_send_message_to_connection(client.get(), message);

                    ++connections_iterator;
                }
                else if (client)
                    connections_iterator = remove_connection(connections_iterator);
            }
        }

        Delegate<Client_connection_interface<Id_type>, bool&> m_on_client_connect;
        Delegate<uint32_t, std::string_view> m_on_client_disconnect;
        Delegate<Client_connection_interface<Id_type>, Net_message<Id_type>> m_on_message;

    protected:
        bool should_stop_wait() noexcept override
        {
            bool parent_conditions = Net_user<Id_type>::should_stop_wait();

            return parent_conditions || !m_new_connections.empty();
        }

    private:
        void handle_received_messages(size_t max_messages)
        {
            for (size_t i = 0; i < max_messages && !this->is_in_queue_empty(); ++i)
            {
                Owned_message<Id_type> owned_message = this->in_queue_pop_front();
                Client_connection_interface<Id_type> connection_interface(owned_message.m_owner);
                m_on_message.broadcast(connection_interface, std::move(owned_message.m_message));
            }
        }

        void handle_new_connections(size_t max_amount)
        {
            for (size_t i = 0; i < max_amount && !m_new_connections.empty(); ++i)
            {
                Protocol::socket socket = m_new_connections.pop_front();

                if (!socket.is_open())
                    continue;

                const std::string ip = socket.remote_endpoint().address().to_string();

                Client_connection_ptr new_connection = this->create_connection<Client_connection>(std::move(socket));

                bool client_accepted = true;
                m_on_client_connect.broadcast(
                    Client_connection_interface<Id_type>(new_connection), std::ref(client_accepted));

                if (client_accepted)
                {
                    const uint32_t id_for_client = m_id_counter++;

                    m_connections.insert(new_connection);
                    new_connection->connect_to_client(id_for_client);

                    this->notifications_push_back(
                        std::format("Client with ip {} was accepted and assigned ip {} to it", ip, id_for_client));
                }
                else
                    this->notifications_push_back(std::format("Connection {} denied", ip));
            }
        }

        void async_wait_for_connections()
        {
            m_acceptor.async_accept([this](asio::error_code error, Protocol::socket socket) {
                if (!error)
                {
                    const std::string ip = socket.remote_endpoint().address().to_string();
                    this->notifications_push_back(std::format("Server new connection: {}", ip));

                    if (!m_banned_ip.contains(ip))
                    {
                        m_new_connections.push_back(std::move(socket));
                        this->notify_wait();
                    }
                    else
                        this->notifications_push_back(std::format("Client with ip {} is banned", ip));
                }
                else
                    this->notifications_push_back(
                        std::format("Server connection error: {}", error.message()), Severity::error);

                async_wait_for_connections();
            });
        }

        template <typename Connection_it_type>
        auto remove_connection(Connection_it_type connection_it)
        {
            Client_connection_ptr client = *connection_it;

            const uint32_t id = client->get_id();
            const std::string ip = client->get_ip().data();

            auto next_it = m_connections.erase(connection_it);

            this->notifications_push_back(std::format("Client disconnected ip: {} id: {}", ip, id));

            m_on_client_disconnect.broadcast(id, ip);

            return next_it;
        }

        void on_new_accepted_message(Id_type type, Message_limits limits) override
        {
            for (Client_connection_ptr client : m_connections)
                client->add_accepted_message(type, limits);
        }

        void check_connections() override
        {
            auto connections_iterator = m_connections.begin();
            while (connections_iterator != m_connections.end())
            {
                Client_connection_ptr client = *connections_iterator;

                if (!client->is_connected())
                    connections_iterator = remove_connection(connections_iterator);
                else
                    ++connections_iterator;
            }
        }

        std::unordered_set<Client_connection_ptr> m_connections;
        Thread_safe_deque<Protocol::socket> m_new_connections;

        Protocol::acceptor m_acceptor;
        uint32_t m_id_counter = Client_id_start;

        std::unordered_set<std::string> m_banned_ip;
    };
} // namespace Net
