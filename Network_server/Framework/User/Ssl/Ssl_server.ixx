export module Network_framework:Ssl_server;

import :Server;
import :Common;

export namespace Net
{
    template <Id_concept Id_type>
    class Ssl_server : public Server<Id_type>
    {
    public:
        Ssl_server(uint16_t port) : Server<Id_type>(port), m_ssl_context(asio::ssl::context::sslv23)
        {
            m_ssl_context.set_password_callback(
                [this](std::size_t size, asio::ssl::context_base::password_purpose purpose) {
                    return get_password(size, purpose);
                });
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

    private:
        /**
         *   Gets ssl password
         *   todo needs actual way to add passwords to server
         */
        [[nodiscard]] std::string get_password(
            [[maybe_unused]] std::size_t size, [[maybe_unused]] asio::ssl::context_base::password_purpose purpose) const
        {
            return "temp password";
        }

        [[nodiscard]] std::unique_ptr<Socket_interface> create_socket_interface(Protocol::socket socket) override
        {
            return std::make_unique<Template_socket<Ssl_socket>>(Ssl_socket(std::move(socket), m_ssl_context));
        }

        asio::ssl::context m_ssl_context;
    };

} // namespace Net
