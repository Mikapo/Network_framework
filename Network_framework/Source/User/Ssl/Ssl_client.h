#include "../Client.h"



namespace Net
{
    template <Id_concept Id_type>
    class Ssl_client : public Client<Id_type>
    {
    public:
        Ssl_client() noexcept : m_ssl_context(asio::ssl::context_base::sslv23)
        {
            m_ssl_context.set_verify_mode(asio::ssl::context_base::verify_peer);
        }

        // Sets ssl verify file
        void set_ssl_verify_file(const std::string& path)
        {
            m_ssl_context.load_verify_file(path);
        }

    private:
        [[nodiscard]] std::unique_ptr<Socket_interface> create_socket_interface(Protocol::socket socket) override
        {
            return std::make_unique<Template_socket<Ssl_socket>>(Ssl_socket(std::move(socket), m_ssl_context));
        }

        asio::ssl::context m_ssl_context;
    };

} // namespace Net