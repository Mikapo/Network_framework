#pragma once

#include "Message.h"

namespace Net
{
    struct Server_data
    {
        uint32_t m_client_id = 0;
    };

    // Static class that is used internally by the framework
    template <Id_concept Id_type>
    class Message_converter
    {
    public:
        Message_converter() = delete;

        // Creates message for server_data
        static Message<Id_type> create_server_accept(const Server_data& data)
        {
            Message<Id_type> output;
            output.set_internal_id(Internal_id::server_accept);
            output << data;
            return output;
        }

        /**
         *	@param	the message that was created with the Create_server_accept method
         *	@throws if the message internal id is not the server_connection_accepted
         *	@return data from the message in the Serverdata struct
         */
        static Server_data extract_server_accept(Message<Id_type>& in_message)
        {
            if (in_message.get_internal_id() != Internal_id::server_accept)
                throw std::invalid_argument("Message has wrong id");

            Server_data output;
            in_message >> output;
            return output;
        }
    };
} // namespace Net
