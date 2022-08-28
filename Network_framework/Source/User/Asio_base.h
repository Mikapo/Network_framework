#pragma once

#include "../Utility/Common.h"

namespace Net
{
    // Class spesifically for handling asio
    class Asio_base
    {
    public:
        Asio_base() noexcept : m_ssl_context(asio::ssl::context::sslv23)
        {
        }

        // Sets ssl certificate chain file
        void set_ssl_certificate_chain_file(const std::string& path)
        {
            m_ssl_context.use_certificate_chain_file(path);
        }

        // Sets ssl private key file in .pem format
        void set_ssl_private_key_file(const std::string& path)
        {
            m_ssl_context.use_rsa_private_key_file(path, asio::ssl::context::pem);
        }

        // Sets ssl tmp dh file
        void set_ssl_tmp_dh_file(const std::string& path)
        {
            m_ssl_context.use_tmp_dh_file(path);
        }

        // Sets ssl verify file
        void set_ssl_verify_file(const std::string& path)
        {
            m_ssl_context.load_verify_file(path);
        }

    protected:
        template <typename Verify_type>
        void set_ssl_verify_mode(Verify_type new_verify_mode)
        {
            m_ssl_context.set_verify_mode(new_verify_mode);
        }

        // Sets ssl pasword callback
        template <typename Func_type>
        void set_ssl_password_callback(const Func_type& func)
        {
            m_ssl_context.set_password_callback(func);
        }

        [[nodiscard]] Protocol::resolver create_resolver()
        {
            return Protocol::resolver(m_asio_context);
        }

        [[nodiscard]] Protocol::acceptor create_acceptor(const Protocol::endpoint& endpoint)
        {
            return Protocol::acceptor(m_asio_context, endpoint);
        }

        [[nodiscard]] Encrypted_socket create_encrypted_socket()
        {
            return Encrypted_socket(m_asio_context, m_ssl_context);
        }

        [[nodiscard]] Encrypted_socket create_encrypted_socket(Protocol::socket socket)
        {
            return Encrypted_socket(std::move(socket), m_ssl_context);
        }

        [[nodiscard]] Protocol::socket create_socket()
        {
            return Protocol::socket(m_asio_context);
        }

        /**
         *   Gives async job to the Asio
         *
         *   @throws if the asio thread is not running
         */
        template <typename Func_type>
        void give_job_to_asio(Func_type job)
        {
            if (!m_asio_thread_stop_flag)
                asio::post(m_asio_context, std::forward<Func_type>(job));
            else
                throw std::logic_error("Asio thread was not running");
        }

        /**
         *   Starts the asio thread and setups the Asio to handle async task'
         *
         *   @throws if the Asio thread was already running
         */
        void start_asio_thread()
        {
            if (!m_asio_thread_handle.joinable())
            {
                if (m_asio_context.stopped())
                    m_asio_context.restart();

                m_asio_thread_stop_flag = false;
                m_asio_thread_handle = std::thread([this] { asio_thread(); });
            }
            else
                throw std::logic_error("Asio thread was already running");
        }

        // Stops the Asio context and the Thread
        void stop_asio_thread()
        {
            if (!m_asio_thread_stop_flag)
            {
                m_asio_thread_stop_flag = true;
                m_asio_context.stop();

                if (m_asio_thread_handle.joinable())
                    m_asio_thread_handle.join();
            }
        }

    private:
        // Seperate thread for running the Asio async
        void asio_thread()
        {
            while (!m_asio_thread_stop_flag)
                m_asio_context.run();
        }

        asio::io_context m_asio_context;
        asio::ssl::context m_ssl_context;

        std::thread m_asio_thread_handle;
        bool m_asio_thread_stop_flag = true;
    };
}; // namespace Net
