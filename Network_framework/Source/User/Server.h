#pragma once

#include "../Utility/Owned_message.h"
#include "User.h"
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace Net
{
    template <Id_concept Id_type>
    class Server : public User<Id_type>
    {
    public:
        using Connection_container = std::unordered_map<uint32_t, std::unique_ptr<Connection<Id_type>>>;

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
            User<Id_type>::update(max_handled_items, wait, check_connections_interval);

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

        void disconnect_client(uint32_t client_id)
        {
            auto found_client = m_connections.find(client_id);

            if (found_client != m_connections.end())
                remove_connection(found_client);
        }

        void send_message_to_client(uint32_t client_id, const Message<Id_type>& message)
        {
            auto found_client = m_connections.find(client_id);
            if (found_client == m_connections.end())
                return;

            auto& client_ptr = found_client->second;

            if (client_ptr->is_connected())
                this->async_send_message_to_connection(client_ptr.get(), message);
            else
                remove_connection(found_client);
        }

        void send_message_to_all_clients(const Message<Id_type>& message, uint32_t ignored_client = 0)
        {
            auto connections_iterator = m_connections.begin();
            while (connections_iterator != m_connections.end())
            {
                const auto& client = connections_iterator->second;

                if (client->is_connected())
                {
                    if (client->get_id() != ignored_client)
                        this->async_send_message_to_connection(client.get(), message);

                    ++connections_iterator;
                }
                else
                    connections_iterator = remove_connection(connections_iterator);
            }
        }

        Delegate<const Client_information&, bool&> m_on_client_connect;
        Delegate<const Client_information&> m_on_client_disconnect;
        Delegate<const Client_information&, Message<Id_type>> m_on_message;

    protected:
        bool should_stop_wait() noexcept override
        {
            bool parent_conditions = User<Id_type>::should_stop_wait();

            return parent_conditions || !m_new_connections.empty();
        }

    private:
        void handle_received_messages(size_t max_messages)
        {
            for (size_t i = 0; i < max_messages && !this->is_in_queue_empty(); ++i)
            {
                Owned_message<Id_type> owned_message = this->in_queue_pop_front();
                m_on_message.broadcast(
                    std::move(owned_message.m_client_information), std::move(owned_message.m_message));
            }
        }

        void handle_new_connections(size_t max_amount)
        {
            for (size_t i = 0; i < max_amount && !m_new_connections.empty(); ++i)
                add_connection(m_new_connections.pop_front());
        }

        void add_connection(Protocol::socket socket)
        {
            if (!socket.is_open())
                return;

            const uint32_t id_for_client = m_id_counter++;
            const std::string ip = socket.remote_endpoint().address().to_string();

            auto new_connection = this->create_connection(std::move(socket), id_for_client);

            bool client_accepted = true;
            m_on_client_connect.broadcast(Client_information(id_for_client, ip), client_accepted);

            if (client_accepted)
            {
                m_connections.emplace(id_for_client, std::move(new_connection));
                this->notifications_push_back(
                    std::format("Client with ip {} was accepted and assigned ip {} to it", ip, id_for_client));
            }
            else
                this->notifications_push_back(std::format("Connection {} denied", ip));
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
            const auto& client = connection_it->second;

            const uint32_t id = client->get_id();
            const std::string ip = client->get_ip().data();

            auto next_it = m_connections.erase(connection_it);

            this->notifications_push_back(std::format("Client disconnected ip: {} id: {}", ip, id));

            m_on_client_disconnect.broadcast(Client_information(id, ip));

            return next_it;
        }

        void check_connections() override
        {
            auto connections_iterator = m_connections.begin();
            while (connections_iterator != m_connections.end())
            {
                const auto& client = connections_iterator->second;

                if (!client->is_connected())
                    connections_iterator = remove_connection(connections_iterator);
                else
                    ++connections_iterator;
            }
        }

        Connection_container m_connections;
        Thread_safe_deque<Protocol::socket> m_new_connections;

        Protocol::acceptor m_acceptor;
        constexpr static uint32_t FIRST_CLIENT_ID = 1000;
        uint32_t m_id_counter = FIRST_CLIENT_ID;

        std::unordered_set<std::string> m_banned_ip;
    };
} // namespace Net
