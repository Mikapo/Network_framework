#pragma once

#include <cstdint>
#include <type_traits>

namespace Net
{
    // Used to handle messages that are internat to framework
    enum class Internal_id : uint8_t
    {
        not_internal,
        server_accept
    };

    // Type that is used to indicate how large the message is in the header
    using Header_size_type = uint64_t;

    // The concept for the message id type
    template <typename T>
    concept Id_concept = std::is_enum_v<T>;

    // The message header is for identifying what type of message has been received
    template <Id_concept Id_type>
    class Message_header
    {
    public:
        // Key used to validate the message
        uint64_t m_validation_key = CONSTANT_VALIDATION_KEY;

        // Spesifies if this message is internal to the framework and not send by the client code
        Internal_id m_internal_id = Internal_id::not_internal;

        // Id used to recognize what type of message this is
        Id_type m_id = {};

        // Size of the message
        Header_size_type m_size = 0;

        [[nodiscard]] bool is_validation_key_correct() const noexcept
        {
            return m_validation_key == CONSTANT_VALIDATION_KEY;
        }

        bool operator==(const Message_header& other) const noexcept
        {
            return m_id == other.m_id && m_size == other.m_size;
        }

        bool operator!=(const Message_header& other) const noexcept
        {
            return !(*this == other);
        }

    private:
        constexpr static uint64_t CONSTANT_VALIDATION_KEY = 9970951313928774000ull;
    };
} // namespace Net
