#pragma once

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>
#include <functional>
#include <memory>

namespace arena60 {

class MetricsHttpServer : public std::enable_shared_from_this<MetricsHttpServer> {
   public:
    using RequestHandler =
        std::function<boost::beast::http::response<boost::beast::http::string_body>(
            const boost::beast::http::request<boost::beast::http::string_body>&)>;

    MetricsHttpServer(boost::asio::io_context& io_context, std::uint16_t port,
                      RequestHandler handler);
    ~MetricsHttpServer();

    void Start();
    void Stop();

    std::uint16_t Port() const;

   private:
    class Session;

    void DoAccept();

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> running_{false};
    RequestHandler handler_;
};

}  // namespace arena60
