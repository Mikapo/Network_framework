#pragma once

#include "Client_information.h"
#include "Net_message.h"

namespace Net
{
    template <Id_concept Id_type>
    struct Owned_message
    {

        Owned_message(Net_message<Id_type> message, Client_information client_information)
            : m_message(std::move(message)), m_client_information(std::move(client_information))
        {
        }

        friend std::ostream& operator<<(std::ostream& stream, const Owned_message& message)
        {
            return stream << message.m_message;
        }

        [[nodiscard]] bool operator==(const Owned_message& other) const noexcept
        {
            return m_client_information == other.m_client_information && m_message == other.m_message;
        }

        [[nodiscard]] bool operator!=(const Owned_message& other) const noexcept
        {
            return !(*this == other);
        }

        Net_message<Id_type> m_message;
        const Client_information m_client_information;
    };
} // namespace Net
