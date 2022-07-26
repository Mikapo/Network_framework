#pragma once

#include "../Connection/Client_connection.h"

namespace Net
{
    template <Id_concept Id_type>
    class Server_interface;

    template <Id_concept Id_type>
    class Client_connection_interface
    {
    public:
        friend Server_interface<Id_type>;
        using Client_connection_ptr = std::shared_ptr<Client_connection<Id_type>>;

        Client_connection_interface(Client_connection_ptr ptr) : m_client_connection(ptr)
        {
        }

        void disconnect()
        {
            if (m_client_connection)
                m_client_connection->disconnect();
        }

        [[nodiscard]] bool is_connected() const
        {
            if (m_client_connection)
                return m_client_connection->is_connected();

            return false;
        }

        [[nodiscard]] uint32_t get_id() const noexcept
        {
            if (m_client_connection)
                return m_client_connection->get_id();

            return 0;
        }

        operator bool() const noexcept
        {
            return m_client_connection.get();
        }

    private:
        Client_connection_ptr get_underlying() const noexcept
        {
            return m_client_connection;
        }

        Client_connection_ptr m_client_connection;
    };
} // namespace Net
