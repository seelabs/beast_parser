#include <beast/http.hpp>
#include <boost/asio.hpp>
#include <iostream>

namespace asio = boost::asio;
namespace http = beast::http;

using boost::asio::ip::tcp;
using namespace std::placeholders;

class connection
        : boost::noncopyable, public std::enable_shared_from_this<connection>
{
    public:
        explicit connection(tcp::socket&& socket)
            : socket_(std::move(socket))
        {}

        void start()
        {
            std::cerr << "New connection\n";
            read_header();
        }

    private:
        using header_parser = http::header_parser_v1<true, http::fields>;
        using body_parser = http::parser_v1<true, http::string_body, http::fields>;

        void read_header()
        {
            std::cerr << "\nWaiting for request...\n";
            buffer_.reset(new asio::streambuf);
            header_parser_.reset(new header_parser);
            async_parse(socket_, *buffer_, *header_parser_, bind(&connection::on_headers_read, shared_from_this(), _1));
        }

        void on_headers_read(const beast::error_code& ec)
        {
            if (ec)
                return close(ec);

            std::cerr << "Received headers:\n" << header_parser_->get();
            std::cerr << "Buffer size after headers: " << buffer_->size() << "\n";

            read_body();
        }

        void read_body()
        {
            body_parser_.reset(new body_parser(*header_parser_));
            async_parse(socket_, *buffer_, *body_parser_, bind(&connection::on_body_read, shared_from_this(), _1));
        }

        void on_body_read(const beast::error_code& ec)
        {
            if (ec)
                return close(ec);

            std::cerr << "Body size: " << body_parser_->get().body.size() << "\n";

            send_response();
        }

        void send_response()
        {
            http::response<http::empty_body> res;
            res.version = body_parser_->get().version;
            res.status = 204;
            res.reason = "OK";
            prepare(res);
            async_write(socket_, move(res), bind(&connection::on_response_sent, shared_from_this(), _1));
        }

        void on_response_sent(const beast::error_code& ec)
        {
            if (ec)
                return close(ec);

            read_header();
        }

        void close(const beast::error_code& ec)
        {
            if (ec)
                std::cerr << "Error: " << ec.message() << "\n";
            socket_.close();
            std::cerr << "Connection closed\n\n";
        }

        tcp::socket                      socket_;
        std::unique_ptr<asio::streambuf> buffer_;
        std::unique_ptr<header_parser>   header_parser_;
        std::unique_ptr<body_parser>     body_parser_;
    };

class server : boost::noncopyable
{
    public:
        server(asio::io_service& ios, const std::string& address, const std::string& port)
            : acceptor_(ios), socket_(ios)
        {
            tcp::resolver resolver(acceptor_.get_io_service());
            tcp::endpoint endpoint = *resolver.resolve({address, port});
            acceptor_.open(endpoint.protocol());
            acceptor_.set_option(tcp::acceptor::reuse_address(true));
            acceptor_.bind(endpoint);
            acceptor_.listen();

            do_accept();
        }

    private:
        void do_accept()
        {
            acceptor_.async_accept(socket_, bind(&server::on_accept, this, _1));
        }

        void on_accept(const boost::system::error_code& ec)
        {
            if (ec)
                throw boost::system::system_error(ec);

            std::make_shared<connection>(std::move(socket_))->start();
            do_accept();
        }

        tcp::acceptor acceptor_;
        tcp::socket   socket_;
};

int main()
{
    try
    {
        asio::io_service ios;
        server s(ios, "0.0.0.0", "8081");
        ios.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << "\n";
    }
}
