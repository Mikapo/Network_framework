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

namespace Net
{
    constexpr uint32_t Client_id_start = 1000;

    template <Id_concept Id_type>
    class Server : public Net_user<Id_type>
    {
    public:
        using Client_connection = Client_connection<Id_type>;
        using Client_connection_ptr = std::shared_ptr<Client_connection>;

        Server(uint16_t port) : m_acceptor(this->create_acceptor(Protocol::endpoint(Protocol::v4(), port)))
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

        void send_message_to_client(Client_connection_ptr client, const Net_message<Id_type>& message)
        {
            if (client && client->is_connected())
                this->async_send_message_to_connection(client.get(), message);
            else
            {
                std::scoped_lock(m_connections_mutext);

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

        void handle_received_messages(size_t max_messages = std::numeric_limits<size_t>::max(), bool wait = true)
        {
            if (wait)
                this->wait_until_has_messages();

            for (size_t i = 0; i < max_messages && !this->is_in_queue_empty(); ++i)
            {
                auto message = this->in_queue_pop_front();
                on_message(message.m_owner, std::move(message.m_message));
            }
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
        void async_wait_for_connections()
        {
            m_acceptor.async_accept([this](asio::error_code error, Protocol::socket socket) {
                if (!error)
                {
                    std::scoped_lock lock(m_connections_mutext);

                    this->on_notification(
                        std::format("Server new connection: {}", socket.remote_endpoint().address().to_string()));

                    Client_connection_ptr new_connection =
                        this->create_connection<Client_connection>(std::move(socket));

                    if (on_client_connect(new_connection))
                    {
                        m_connections.push_back(new_connection);
                        m_connections.back()->connect_to_client(m_id_counter++);
                        this->on_notification(
                            std::format("Client with id {} was accepted", m_connections.back()->get_id()));
                    }
                    else
                    {
                        this->on_notification("Server connection denied");
                    }
                }
                else
                    this->on_notification(std::format("Server connection error: {}", error.message()), Severity::error);

                async_wait_for_connections();
            });
        }

        void notify_client_disconnect(Client_connection_ptr client)
        {
            this->on_notification(std::format("Client disconnected"));
            on_client_disconnect(client);
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
    };
} // namespace Net
