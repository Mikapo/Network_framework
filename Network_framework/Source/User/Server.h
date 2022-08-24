#pragma once

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
        using Optional_seconds = std::optional<std::chrono::seconds>;

        explicit Server(uint16_t port) : m_acceptor(this->create_acceptor(Protocol::endpoint(Protocol::v4(), port)))
        {
            this->set_ssl_password_callback([this](std::size_t size, asio::ssl::context_base::password_purpose purpose) {
                return get_password(size, purpose);
            });
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

        /**
         *   Handle everything received through internet
         *
         *   @param The max items handled
         *   @param Should the function wait if there is no items to handle
         *   @param Optional interval for checking connections. If you don't give this there will be no checking
         */
        void update(
            size_t max_handled_items = SIZE_T_MAX, bool wait = false,
            Optional_seconds check_connections_interval = Optional_seconds()) override
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
            auto found_client = m_clients.find(client_id);

            if (found_client != m_clients.end())
                remove_client(found_client);
        }

        void send_message_to_client(uint32_t client_id, Message<Id_type> message)
        {
            auto found_client = m_clients.find(client_id);
            if (found_client == m_clients.end())
                return;

            const auto& connection_ptr = found_client->second.m_connection;

            if (connection_ptr->is_connected())
                this->async_send_message_to_connection(connection_ptr.get(), std::move(message));
            else
                remove_client(found_client);
        }

        void send_message_to_all_clients(const Message<Id_type>& message, uint32_t ignored_client = 0)
        {
            auto client_iterator = m_clients.begin();
            while (client_iterator != m_clients.end())
            {
                const auto& connection = client_iterator->second.m_connection;

                if (connection->is_connected())
                {
                    if (connection->get_id() != ignored_client)
                        this->async_send_message_to_connection(connection.get(), message);

                    ++client_iterator;
                }
                else
                    client_iterator = remove_client(client_iterator);
            }
        }

        Delegate<const Client_information&, bool&> m_on_client_connect;
        Delegate<const Client_information&> m_on_client_disconnect;
        Delegate<const Client_information&, Message<Id_type>> m_on_message;

    protected:
        bool should_stop_waiting() override
        {
            const bool parent_conditions = User<Id_type>::should_stop_waiting();

            return parent_conditions || !m_new_connections.empty();
        }

    private:
        struct Client_data
        {
            std::unique_ptr<Connection<Id_type>> m_connection = nullptr;
        };

        /**
         *   Gets ssl password
         *   todo needs actual way to add passwords to server
         */
        [[nodiscard]] std::string get_password(
            [[maybe_unused]] std::size_t size, [[maybe_unused]] asio::ssl::context_base::password_purpose purpose) const
        {
            return "temp password";
        }

        // Triggers the on message callback for the every message
        void handle_received_messages(size_t max_messages)
        {
            for (size_t i = 0; i < max_messages && !this->is_in_queue_empty(); ++i)
            {
                Owned_message<Id_type> owned_message = this->in_queue_pop_front();

                if (owned_message.m_message.get_internal_id() == Internal_id::not_internal)
                    m_on_message.broadcast(
                        std::move(owned_message.m_client_information), std::move(owned_message.m_message));
                else
                    handle_internal_message(std::move(owned_message));
            }
        }

        // Handles messages internal to framework
        void handle_internal_message(Owned_message<Id_type> message)
        {
            // Currently we have no internal messages in the server so this must be invalid message
            disconnect_client(message.m_client_information.m_id);
        }

        /**
         * Handles the new non accepted connections
         *
         * @param max amount of the new connections handled
         */
        void handle_new_connections(size_t max_amount)
        {
            for (size_t i = 0; i < max_amount && !m_new_connections.empty(); ++i)
                create_client(m_new_connections.pop_front());
        }

        // Prepares the client for receiving messages
        void setup_client(std::unique_ptr<Connection<Id_type>> connection, uint32_t unique_id)
        {
            auto accept_message = Message_converter<Id_type>::create_server_data({unique_id});
           // this->async_send_message_to_connection(connection.get(), std::move(accept_message));

            Client_data client = {std::move(connection)};
            m_clients.emplace(unique_id, std::move(client));
        }

        // Adds the new socket as connection
        void create_client(Protocol::socket socket)
        {
            if (!socket.is_open())
                return;

            const uint32_t client_id = m_id_counter++;
            const std::string client_ip = socket.remote_endpoint().address().to_string();

            bool client_accepted = true;
            m_on_client_connect.broadcast(Client_information(client_id, client_ip), client_accepted);

            if (client_accepted)
            {
                auto new_connection =
                    this->create_connection_from_socket(std::move(socket), client_id, Handshake_type::server);

                this->notifications_push_back(
                    std::format("Client with ip {} was accepted and assigned ip {} to it", client_ip, client_id));

                setup_client(std::move(new_connection), client_id);
            }
            else
                this->notifications_push_back(std::format("Connection {} denied", client_ip));
        }

        // Primes the Asio thread to wait for the connections in async way
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

        /**
         *   Removes the client from m_client
         *
         *   @param The iterator pointing to the client in m_clients
         *   @return The iterator pointing to the next element after removed client
         */
        template <typename Client_it_type>
        auto remove_client(Client_it_type client_it)
        {
            const auto& connection = client_it->second.m_connection;

            const uint32_t id = connection->get_id();
            const std::string ip = connection->get_ip().data();

            auto next_it = m_clients.erase(client_it);

            this->notifications_push_back(std::format("Client disconnected ip: {} id: {}", ip, id));

            m_on_client_disconnect.broadcast(Client_information(id, ip));

            return next_it;
        }

        // Removes all the unconnected clients
        void check_connections() override
        {
            auto clients_iterator = m_clients.begin();
            while (clients_iterator != m_clients.end())
            {
                const auto& connection = clients_iterator->second.m_connection;

                if (!connection->is_connected())
                    clients_iterator = remove_client(clients_iterator);
                else
                    ++clients_iterator;
            }
        }

        std::unordered_map<uint32_t, Client_data> m_clients;
        Thread_safe_deque<Protocol::socket> m_new_connections;

        Protocol::acceptor m_acceptor;
        uint32_t m_id_counter = 1000;

        std::unordered_set<std::string> m_banned_ip;
    };
} // namespace Net
