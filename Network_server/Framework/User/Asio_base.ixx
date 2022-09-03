module;
#include "../Utility/Asio_includes.h"

export module Network_framework:Asio_base;
import :Common;

namespace Net
{
    // Class spesifically for handling asio
    class Asio_base
    {
    protected:
        [[nodiscard]] Protocol::resolver create_resolver()
        {
            return Protocol::resolver(m_asio_context);
        }

        [[nodiscard]] Protocol::acceptor create_acceptor(const Protocol::endpoint& endpoint)
        {
            return Protocol::acceptor(m_asio_context, endpoint);
        }

        [[nodiscard]] Protocol::socket create_socket()
        {
            return Protocol::socket(m_asio_context);
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
        std::thread m_asio_thread_handle;
        bool m_asio_thread_stop_flag = true;
    };
}; // namespace Net
