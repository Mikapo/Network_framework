#pragma once

#include "../Utility/Net_common.h"
#include <type_traits>
#include <vector>
#include <ostream>
#include <stdexcept>
#include <span>
#include "gsl/span"
#include "gsl/gsl"

namespace Net
{
	template<Id_concept Id_type>
	struct Net_message_header
	{
		const uint64_t m_validation_key = VALIDATION_KEY;
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

	template<Id_concept Id_type>
	struct Net_message
	{
		friend std::ostream& operator<<(std::ostream& stream, const Net_message& message)
		{
			stream << "ID: " << static_cast<std::underlying_type_t<Id_type>>(message.m_header.m_id) << " Size: " << message.m_header.m_size;
			return stream;
		}

		template<typename Data_type>
		Net_message& push_back(const Data_type& data) requires(std::is_standard_layout_v<Data_type>)
		{
			const size_t size = m_body.size();
			const size_t new_size = size + sizeof(Data_type);

			resize_body(new_size);

			const gsl::span body_span = {m_body.data(), m_body.size()};
			std::memcpy(&body_span[size], &data, sizeof(Data_type));

			m_header.m_size = gsl::narrow<uint32_t>(m_body.size());
			return *this;
		}

		template<typename Data_type>
		Net_message& extract(Data_type& data) requires(std::is_standard_layout_v<Data_type>)
		{
			if (sizeof(Data_type) > m_body.size())
				throw std::length_error("not enough data to extract");

			const size_t new_size = m_body.size() - sizeof(Data_type);

			const gsl::span body_span = { m_body.data(), m_body.size() };
			std::memcpy(&data, &body_span[new_size], sizeof(Data_type));

			resize_body(new_size);
			m_header.m_size = gsl::narrow<uint32_t>(m_body.size());
			return *this;
		}

		template<typename Data_type>
		friend Net_message& operator<<(Net_message& message, const Data_type& data) requires(std::is_standard_layout_v<Data_type>)
		{
			return message.push_back(data);
		}

		template<typename Data_type>
		friend Net_message& operator>>(Net_message& message, Data_type& data) requires(std::is_standard_layout_v<Data_type>)
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

		void resize_body(size_t new_size)
		{
			m_body.resize(new_size);
		}

		bool is_empty() const noexcept
		{
			return m_body.size() == 0;
		}

		size_t size_in_bytes() const noexcept
		{
			return sizeof(m_header) + m_body.size();
		}

		Net_message_header<Id_type> m_header;
		std::vector<char> m_body = {};
	};

	template<Id_concept Id_type>
	class Client_connection;

	template<Id_concept Id_type>
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

		Client_connection_ptr m_owner = nullptr;
		Net_message<Id_type> m_message;
	};

};

