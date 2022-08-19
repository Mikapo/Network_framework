#pragma once

#include <compare>
#include <cstdint>
#include <string_view>

namespace Net
{
    struct Client_information
    {
        Client_information() = default;
        Client_information(uint32_t client_id, std::string_view client_ip) : m_id(client_id), m_ip(client_ip)
        {
        }

        auto operator<=>(const Client_information&) const = default;

        uint32_t m_id = 0;
        std::string m_ip = "0.0.0.0";
    };
} // namespace Net
