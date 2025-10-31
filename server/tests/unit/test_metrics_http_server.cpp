#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/system/error_code.hpp>
#include <future>
#include <string>
#include <thread>
#include <utility>

#include "arena60/network/metrics_http_server.h"

namespace {
std::pair<boost::beast::http::status, std::string> PerformRequest(std::uint16_t port,
                                                                  const std::string& target) {
    boost::asio::io_context client_io;
    boost::beast::tcp_stream stream(client_io);
    boost::asio::ip::tcp::resolver resolver(client_io);
    auto results = resolver.resolve("127.0.0.1", std::to_string(port));
    stream.connect(results);

    namespace http = boost::beast::http;
    http::request<http::string_body> req{http::verb::get, target, 11};
    req.set(http::field::host, "127.0.0.1");
    http::write(stream, req);

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    boost::system::error_code ec;
    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    stream.socket().close(ec);

    return {res.result(), res.body()};
}
}  // namespace

TEST(MetricsHttpServerTest, ServesMetricsPayload) {
    boost::asio::io_context io_context;
    std::promise<void> ready;
    std::shared_ptr<arena60::MetricsHttpServer> server;
    std::thread server_thread([&]() {
        server = std::make_shared<arena60::MetricsHttpServer>(
            io_context, 0, []() { return std::string{"metric 1\n"}; });
        server->Start();
        ready.set_value();
        io_context.run();
    });

    ready.get_future().wait();
    const auto port = server->Port();
    ASSERT_NE(port, 0);

    const auto metrics_response = PerformRequest(port, "/metrics");
    EXPECT_EQ(boost::beast::http::status::ok, metrics_response.first);
    EXPECT_EQ("metric 1\n", metrics_response.second);

    const auto not_found = PerformRequest(port, "/unknown");
    EXPECT_EQ(boost::beast::http::status::not_found, not_found.first);

    server->Stop();
    io_context.stop();
    server_thread.join();
}
