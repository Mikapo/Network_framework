#pragma once

#include "../Net_user/Client.h"

namespace Net
{
    template <Id_concept Id_type>
    class Client_interface : private Client<Id_type>
    {
    public:
        using Underlying = Client<Id_type>;

        bool connect(std::string_view host, std::string_view port)
        {
            return this->Underlying::connect(host, port);
        }

        void disconnect()
        {
            this->Underlying::disconnect();
        }

        bool is_connected() const noexcept
        {
            return this->Underlying::is_connected();
        }

        void send_message(const Net_message<Id_type>& message)
        {
            this->Underlying::send_message(message);
        }

        void handle_received_messages(size_t max_messages = std::numeric_limits<size_t>::max())
        {
            this->Underlying::handle_received_messages(max_messages);
        }

        void on_message(Net_message<Id_type>& message) = 0;
    };
} // namespace Net
