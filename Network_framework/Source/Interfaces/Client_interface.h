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

        [[nodiscard]] bool is_connected() const noexcept
        {
            return this->Underlying::is_connected();
        }

        void send_message(const Net_message<Id_type>& message)
        {
            this->Underlying::send_message(message);
        }

        void update(size_t max_handled_items = std::numeric_limits<size_t>::max(), bool wait = false)
        {
            this->Underlying::update(max_handled_items, wait, std::optional<std::chrono::seconds>());
        }

        void add_accepted_message(Id_type type, uint32_t min = 0, uint32_t max = std::numeric_limits<uint32_t>::max())
        {
            this->Underlying::add_accepted_message(type, min, max);
        }

        void on_message(Net_message<Id_type> message) override{};
    };
} // namespace Net
