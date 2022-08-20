#pragma once

#include "../Utility/Net_common.h"
#include <ostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Net
{
    template <Id_concept Id_type>
    class Connection;

    using Header_size_type = uint64_t;

    template <Id_concept Id_type>
    struct Net_message_header
    {
        constexpr static uint64_t CONSTANT_VALIDATION_KEY = 9970951313928774000;

        uint64_t m_validation_key = CONSTANT_VALIDATION_KEY;
        Id_type m_id = {};
        Header_size_type m_size = 0;

        bool operator==(const Net_message_header& other) const noexcept
        {
            return m_id == other.m_id && m_size == other.m_size;
        }

        bool operator!=(const Net_message_header& other) const noexcept
        {
            return !(*this == other);
        }
    };

    template <typename T>
    concept Net_message_data_concept = std::is_standard_layout_v<T>;

    template <Id_concept Id_type>
    class Message
    {
    public:
        using Size_type = uint64_t;
        friend Connection<Id_type>;

        friend std::ostream& operator<<(std::ostream& stream, const Message& message)
        {
            stream << "ID: " << static_cast<std::underlying_type_t<Id_type>>(message.m_header.m_id)
                   << " Size: " << message.m_header.m_size;
            return stream;
        }

        template <Net_message_data_concept Data_type>
        Message& push_back(const Data_type& data)
        {
            const size_t size = m_body.size();
            const size_t new_size = size + sizeof(Data_type);

            if (new_size > std::numeric_limits<Header_size_type>::max())
                throw std::length_error("storing too much data to message");

            resize_body(new_size);

            const std::span body_span = {m_body.data(), m_body.size()};
            std::memcpy(&body_span[size], &data, sizeof(Data_type));

            m_header.m_size = static_cast<Header_size_type>(m_body.size());
            return *this;
        }

        template <typename Iterator_type>
        Message& push_back_from_container(Iterator_type begin, Iterator_type end)
        {
            const size_t size = end - begin;
            m_body.reserve(size);

            for (; begin != end; ++begin)
                push_back(*begin);

            return *this;
        }

        template <Net_message_data_concept Data_type>
        Message& extract(Data_type& data)
        {
            if (sizeof(Data_type) > m_body.size())
                throw std::length_error("not enough data to extract");

            const size_t new_size = m_body.size() - sizeof(Data_type);

            const std::span body_span = {m_body.data(), m_body.size()};
            std::memcpy(&data, &body_span[new_size], sizeof(Data_type));

            resize_body(new_size);
            m_header.m_size = static_cast<Header_size_type>(m_body.size());
            return *this;
        }

        template <typename Iterator_type>
        void extract_to_container(
            Iterator_type begin, Iterator_type end, size_t max_amount = std::numeric_limits<size_t>::max())
        {
            using Type = std::remove_reference_t<decltype(*begin)>;
            constexpr size_t type_size = sizeof(Type);

            for (size_t amount = 0; begin != end; ++begin, ++amount)
            {
                if (m_body.size() < type_size || amount > max_amount)
                    break;

                Type value;
                extract(value);

                *begin = std::move(value);
            }
        }

        // pushes size of string as last value in uint64_t format
        void push_back_string(std::string_view string)
        {
            push_back_from_container(string.begin(), string.end());
            push_back(static_cast<Size_type>(string.size()));
        }

        // excepts first value to be uint64_t of size of string
        std::string extract_as_string()
        {
            Size_type string_size;
            extract(string_size);

            if (string_size > m_body.size())
            {
                push_back(string_size);
                throw std::logic_error("Invalid string size");
            }

            std::string output;
            output.resize(string_size);
            extract_to_container(output.rbegin(), output.rend());

            return output;
        }

        template <Net_message_data_concept Data_type>
        friend Message& operator<<(Message& message, const Data_type& data)
        {
            return message.push_back(data);
        }

        template <Net_message_data_concept Data_type>
        friend Message& operator>>(Message& message, Data_type& data)
        {
            return message.extract(data);
        }

        [[nodiscard]] bool operator==(const Message& other) const noexcept
        {
            return m_header == other.m_header && m_body == other.m_body;
        }

        [[nodiscard]] bool operator!=(const Message& other) const noexcept
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
}; // namespace Net
