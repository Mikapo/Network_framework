#pragma once
#pragma once

#include "Net_connection.h"

namespace Net
{
    template <Id_concept Id_type>
    class Server_connection : public Net_connection<Id_type>
    {
    public:
        using Net_connection = Net_connection<Id_type>;

        explicit Server_connection(Protocol::socket socket) : Net_connection(std::move(socket))
        {
        }

        void connect_to_server(const Protocol::resolver::results_type& endpoints)
        {
            this->async_connect(endpoints);
        }
    };
} // namespace Net
