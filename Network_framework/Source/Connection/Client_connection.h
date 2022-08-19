#pragma once

#include "Net_connection.h"
#include <memory>

namespace Net
{
    template <Id_concept Id_type>
    class Client_connection : public Net_connection<Id_type>,
                              private std::enable_shared_from_this<Client_connection<Id_type>>
    {
    public:
        using Client_connection_ptr = std::shared_ptr<Client_connection<Id_type>>;
        using Net_connection = Net_connection<Id_type>;

        explicit Client_connection(Protocol::socket socket) : Net_connection(std::move(socket))
        {
        }

        void connect_to_client(uint32_t id)
        {
            if (this->is_connected())
            {
                m_id = id;
                this->start_waiting_for_messages();
            }
        }

        [[nodiscard]] uint32_t get_id() const noexcept
        {
            return m_id;
        }

    private:
        [[nodiscard]] Owned_message<Id_type> create_owned_message(Net_message<Id_type> message)
        {
            return Owned_message<Id_type>(std::move(message), this->shared_from_this());
        }

        uint32_t m_id = 0;
    };
} // namespace Net
