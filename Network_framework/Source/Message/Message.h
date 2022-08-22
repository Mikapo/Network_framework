#pragma once

#include "../Utility/Common.h"
#include "Message_header.h"
#include <ostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Net
{
    // This concept decides what kind of data the message accepts
    template <typename T>
    concept Message_data_concept = std::is_standard_layout_v<T>;

    template<Id_concept Id_type>
    class Connection;

    // This class is used to store messages that are sent over internet
    template <Id_concept Id_type>
    class Message
    {
    public:
        // Type used to store container sizes in the message body
        using Size_type = uint64_t;

        // Marking connection as friend so it has full access to the body and the header
        friend Connection<Id_type>;

        // Print operator
        friend std::ostream& operator<<(std::ostream& stream, const Message& message)
        {
            stream << "ID: " << static_cast<std::underlying_type_t<Id_type>>(message.m_header.m_id)
                   << " Size: " << message.m_header.m_size;
            return stream;
        }

        /** 
        *   Push data to the end of the message
        * 
        *   @param data to be pushed
        *   @throws if the size of data is larger than the max value that the Header_size_type can hold
        */
        template <Message_data_concept Data_type>
        void push_back(const Data_type& data)
        {
            const size_t size = m_body.size();
            const size_t new_size = size + sizeof(Data_type);

            if (new_size > std::numeric_limits<Header_size_type>::max())
                throw std::length_error("storing too much data to message");

            resize_body(new_size);

            // Copying the data to the end of the body 
            std::memcpy(&m_body.at(size), &data, sizeof(Data_type));

            m_header.m_size = checked_cast<Header_size_type>(m_body.size());
        }

        /**
        *   Extract data from the end of the message
        *   
        *   @return the data that was extracted
        *   @throws if there is not enough data to be extracted
        */
        template <Message_data_concept Data_type>
        Data_type extract()
        {
            if (sizeof(Data_type) > m_body.size())
                throw std::length_error("not enough data to extract");

            const size_t new_size = m_body.size() - sizeof(Data_type);

            // Copyign to the data from the end of the body
            Data_type data;
            std::memcpy(&data, &m_body.at(new_size), sizeof(Data_type));

            resize_body(new_size);
            m_header.m_size = checked_cast<Header_size_type>(m_body.size());

            return data;
        }

        // Pushes a string and the size of the string after the string
        void push_back_string(std::string_view string)
        {
            push_back_from_container(string.begin(), string.end());
            push_back(checked_cast<Size_type>(string.size()));
        }

        // Expects the size of the string at top of the message
        std::string extract_as_string()
        {
            std::string output;
            output.resize(extract<Size_type>());
            extract_to_container(output.rbegin(), output.rend());
            return output;
        }


        // Operator << for pushing data
        template <Message_data_concept Data_type>
        friend Message& operator<<(Message& message, const Data_type& data)
        {
            message.push_back<Data_type>(data);
            return message;
        }

        // Operator >> for extracting data
        template <Message_data_concept Data_type>
        friend Message& operator>>(Message& message, Data_type& data)
        {
            data = message.extract<Data_type>();
            return message;
        }

        [[nodiscard]] bool operator==(const Message& other) const noexcept
        {
            return m_header == other.m_header && m_body == other.m_body;
        }

        [[nodiscard]] bool operator!=(const Message& other) const noexcept
        {
            return !(*this == other);
        }

        /** 
        *   Sets internal id of the message.
        *   This should never be called outside the framework.
        */
        void set_internal_id(Internal_id new_internal_id) noexcept
        {
            m_header.m_internal_id = new_internal_id;
        }

        // Outside of framework this should always be not_internal
        [[nodiscard]] Internal_id get_internal_id() const noexcept
        {
            return m_header.m_internal_id;
        }

        [[nodiscard]] Id_type get_id() const noexcept
        {
            return m_header.m_id;
        }

        void set_id(Id_type new_id) noexcept
        {
            m_header.m_id = new_id;
        }

        void clear() noexcept
        {
            m_body.clear();
            m_header.m_size = 0;
        }

        [[nodiscard]] bool is_empty() const noexcept
        {
            return m_body.empty();
        }

    private:
        /**
         *  @param begin iterator
         *  @param end iterator
         *  @throws if tries to push back too much data
         */
        template <typename Iterator_type>
        void push_back_from_container(Iterator_type begin, Iterator_type end)
        {
            for (; begin != end; ++begin)
                push_back(*begin);
        }

        /**
        *   @param begin iterator
        *   @param end iterator
        *   @throws if tries to extract too much data
        */
        template <typename Iterator_type>
        void extract_to_container(Iterator_type begin, Iterator_type end)
        {
            using Type = std::iterator_traits<Iterator_type>::value_type;

            for (; begin != end; ++begin)
                *begin = extract<Type>();
        }

        void resize_body(size_t new_size)
        {
            m_body.resize(new_size);
        }

        // Simple integral cast with check that the value has not changes after cast
        template <std::integral Cast_to, std::integral Cast_from>
        static Cast_to checked_cast(const Cast_from& value)
        {
            const Cast_to casted_value = static_cast<Cast_to>(value);

            if (value != casted_value)
                throw std::length_error("Value changed when casted");

            return casted_value;
        }

        Message_header<Id_type> m_header;

        // The message body in bytes
        std::vector<char> m_body = {};
    };
} // namespace Net
