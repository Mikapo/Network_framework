#pragma once

#include "../Utility/Net_common.h"
#include <ostream>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace Net
{
    template <Id_concept Id_type>
    class Client_connection;

    template <Id_concept Id_type>
    class Net_connection;

    template <Id_concept Id_type>
    struct Net_message_header
    {
        uint64_t m_validation_key = VALIDATION_KEY;
        Id_type m_id = {};
        uint32_t m_size = 0;

        bool operator==(const Net_message_header& other) const noexcept
        {
            return m_id == other.m_id && m_size == other.m_size;
        }

        bool operator!=(const Net_message_header& other) const noexcept
        {
            return !(*this == other);
        }
    };

    template <Id_concept Id_type>
    class Net_message
    {
    public:
        friend Net_connection<Id_type>;

        friend std::ostream& operator<<(std::ostream& stream, const Net_message& message)
        {
            stream << "ID: " << static_cast<std::underlying_type_t<Id_type>>(message.m_header.m_id)
                   << " Size: " << message.m_header.m_size;
            return stream;
        }

        template <typename Data_type>
        Net_message& push_back(const Data_type& data) requires(std::is_standard_layout_v<Data_type>)
        {
            const size_t size = m_body.size();
            const size_t new_size = size + sizeof(Data_type);

            if (new_size > std::numeric_limits<uint32_t>::max())
                throw std::length_error("storing too much data to message");

            resize_body(new_size);

            const std::span body_span = {m_body.data(), m_body.size()};
            std::memcpy(&body_span[size], &data, sizeof(Data_type));

            m_header.m_size = static_cast<uint32_t>(m_body.size());
            return *this;
        }

        template <typename Iterator_type>
        Net_message& push_back_from_container(Iterator_type begin, Iterator_type end)
        {
            const size_t size = end - begin;
            m_body.reserve(size);

            for (; begin != end; ++begin)
                push_back(*begin);

            return *this;
        }

        template <typename Data_type>
        Net_message& extract(Data_type& data) requires(std::is_standard_layout_v<Data_type>)
        {
            if (sizeof(Data_type) > m_body.size())
                throw std::length_error("not enough data to extract");

            const size_t new_size = m_body.size() - sizeof(Data_type);

            const std::span body_span = {m_body.data(), m_body.size()};
            std::memcpy(&data, &body_span[new_size], sizeof(Data_type));

            resize_body(new_size);
            m_header.m_size = static_cast<uint32_t>(m_body.size());
            return *this;
        }

        template <typename Iterator_type>
        void extract_to_container(
            Iterator_type begin, Iterator_type end, size_t max_amount = std::numeric_limits<size_t>::max())
        {
            using Type = std::remove_reference_t<decltype(*begin)>;
            constexpr size_t type_size = sizeof(Type);

            size_t amount = 0;
            for (; begin != end; ++begin)
            {
                if (m_body.size() < type_size || amount > max_amount)
                    break;

                Type value;
                extract(value);

                *begin = std::move(value);

                ++amount;
            }
        }

        std::string extract_as_string(size_t string_size = std::numeric_limits<size_t>::max())
        {
            std::string output;
            const size_t size = std::min(string_size, m_body.size());
            output.resize(size);
            extract_to_container(output.rbegin(), output.rend());

            return output;
        }

        template <typename Data_type>
        friend Net_message& operator<<(Net_message& message, const Data_type& data) requires(
            std::is_standard_layout_v<Data_type>)
        {
            return message.push_back(data);
        }

        template <typename Data_type>
        friend Net_message& operator>>(Net_message& message, Data_type& data) requires(
            std::is_standard_layout_v<Data_type>)
        {
            return message.extract(data);
        }

        bool operator==(const Net_message& other) const noexcept
        {
            return m_header == other.m_header && m_body == other.m_body;
        }

        bool operator!=(const Net_message& other) const noexcept
        {
            return !(*this == other);
        }

        [[nodiscard]] Id_type get_id() const noexcept
        {
            return m_header.m_id;
        }

        void set_id(Id_type new_id) noexcept
        {
            m_header.m_id = new_id;
        }

        [[nodiscard]] bool is_empty() const noexcept
        {
            return m_body.empty();
        }

        [[nodiscard]] size_t size_in_bytes() const noexcept
        {
            return sizeof(m_header) + m_body.size();
        }

    private:
        void resize_body(size_t new_size)
        {
            m_body.resize(new_size);
        }

        Net_message_header<Id_type> m_header;
        std::vector<char> m_body = {};
    };

    template <Id_concept Id_type>
    struct Owned_message
    {
        using Client_connection_ptr = std::shared_ptr<Client_connection<Id_type>>;

        friend std::ostream& operator<<(std::ostream& stream, const Owned_message& message)
        {
            return stream << message.m_message;
        }

        bool operator==(const Owned_message& other) const noexcept
        {
            return m_owner == other.m_owner && m_message == other.m_message;
        }

        bool operator!=(const Owned_message& other) const noexcept
        {
            return !(*this == other);
        }

        Net_message<Id_type> m_message;
        Client_connection_ptr m_owner = nullptr;
    };
}; // namespace Net
