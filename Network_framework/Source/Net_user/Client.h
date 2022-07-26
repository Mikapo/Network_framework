#pragma once

#include "../Connection/Server_connection.h"
#include "../Utility/Thread_safe_deque.h"
#include "Net_user.h"
#include <cstdint>
#include <memory>

namespace Net
{
    template <Id_concept Id_type>
    class Client : public Net_user<Id_type>
    {
    public:
        using Server_connection = Server_connection<Id_type>;
        using Server_connection_ptr = std::unique_ptr<Server_connection>;

        Client() noexcept : m_socket(this->create_socket())
        {
        }

        ~Client() override
        {
            disconnect();
        }

        Client(const Client&) = delete;
        Client(Client&&) = delete;
        Client& operator=(const Client&) = delete;
        Client& operator=(Client&&) = delete;

        bool connect(std::string_view host, std::string_view port)
        {
            try
            {
                Protocol::resolver resolver = this->create_resolver();
                auto endpoints = resolver.resolve(host, port);

                m_connection = construct_new_connection();
                m_connection->connect_to_server(endpoints);
                this->start_asio_thread();
            }
            catch (const std::exception& exception)
            {
                this->on_notification(std::format("Exception: {}", exception.what()), Severity::error);
                return false;
            }

            return true;
        }

        void disconnect()
        {
            if (is_connected())
                m_connection->disconnect();

            this->stop_asio_thread();
            m_connection.reset();
        }

        [[nodiscard]] bool is_connected() const
        {
            if (m_connection)
                return m_connection->is_connected();

            return false;
        }

        void send_message(const Net_message<Id_type>& message)
        {
            if (is_connected())
                m_connection->async_send_message(message);
        }

        void handle_received_messages(size_t max_messages = std::numeric_limits<size_t>::max())
        {
            for (size_t i = 0; i < max_messages && !this->is_in_queue_empty(); ++i)
            {
                auto message = this->in_queue_pop_front();
                on_message(message.m_message);
            }
        }

    protected:
        [[nodiscard]] Server_connection_ptr construct_new_connection()
        {
            auto give_asio_job_lambda = [this](std::function<void()> job) { this->give_asio_job(job); };
            std::unique_ptr new_connection =
                std::make_unique<Server_connection>(this->create_socket(), give_asio_job_lambda);

            new_connection->set_on_message_received_callback(
                [this](const Net_message<Id_type>& message) { on_message_received(message); });

            new_connection->set_notification_callback([this](const std::string_view message, Severity severity) {
                this->on_notification(message, severity);
            });

            new_connection->set_accepted_messages(this->get_current_accepted_messages());

            return new_connection;
        }

        virtual void on_message(Net_message<Id_type>& message)
        {
        }

    private:
        void on_message_received(const Net_message<Id_type>& message)
        {
            Owned_message<Id_type> owned_message = {.m_owner = nullptr, .m_message = message};
            this->in_queue_push_back(std::move(owned_message));
        }

        void on_new_accepted_message(Id_type type, Message_limits limits) override
        {
            if (m_connection)
                m_connection->add_accepted_message(type, limits);
        }

        Protocol::socket m_socket;
        Server_connection_ptr m_connection;
    };
} // namespace Net
