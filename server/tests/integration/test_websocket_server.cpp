#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include "arena60/core/game_loop.h"
#include "arena60/game/game_session.h"
#include "arena60/network/websocket_server.h"

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

TEST(WebSocketServerIntegrationTest, ProcessesInputAndReturnsState) {
    arena60::GameSession session(60.0);
    arena60::GameLoop loop(60.0);
    boost::asio::io_context io_context;

    auto server = std::make_shared<arena60::WebSocketServer>(io_context, 0, session, loop);
    std::atomic<int> start_events{0};
    std::atomic<int> end_events{0};
    server->SetLifecycleHandlers(
        [&](const std::string& player_id) {
            (void)player_id;
            start_events.fetch_add(1, std::memory_order_relaxed);
        },
        [&](const std::string& player_id) {
            (void)player_id;
            end_events.fetch_add(1, std::memory_order_relaxed);
        });
    server->Start();
    loop.Start();

    std::thread server_thread([&]() { io_context.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto port = server->Port();
    ASSERT_NE(port, 0);

    boost::asio::io_context client_io;
    tcp::resolver resolver(client_io);
    auto results = resolver.resolve("127.0.0.1", std::to_string(port));
    websocket::stream<tcp::socket> ws(client_io);
    boost::asio::connect(ws.next_layer(), results.begin(), results.end());
    ws.handshake("127.0.0.1", "/");

    std::vector<double> rtts_ms;
    rtts_ms.reserve(100);
    for (int i = 0; i < 100; ++i) {
        boost::beast::multi_buffer buffer;
        const auto start = std::chrono::steady_clock::now();
        std::ostringstream frame;
        frame << "input player1 " << (i + 1) << " 1 0 0 0 1.0 0.0";
        ws.write(boost::asio::buffer(frame.str()));
        ws.read(buffer);
        const auto elapsed =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
        rtts_ms.push_back(elapsed.count());

        const std::string payload = boost::beast::buffers_to_string(buffer.data());
        EXPECT_TRUE(payload.rfind("state", 0) == 0);
    }

    boost::system::error_code close_error;
    ws.next_layer().shutdown(tcp::socket::shutdown_both, close_error);
    ws.next_layer().close(close_error);

    server->Stop();
    loop.Stop();
    io_context.stop();
    loop.Join();
    server_thread.join();

    std::sort(rtts_ms.begin(), rtts_ms.end());
    auto percentile = [&rtts_ms](double p) {
        const double rank = (p / 100.0) * static_cast<double>(rtts_ms.size());
        const std::size_t index = static_cast<std::size_t>(std::ceil(rank)) - 1;
        return rtts_ms[std::min(index, rtts_ms.size() - 1)];
    };
    const double p50 = percentile(50.0);
    const double p95 = percentile(95.0);
    const double p99 = percentile(99.0);
    std::cout << "RTT percentiles (ms): p50=" << p50 << " p95=" << p95 << " p99=" << p99
              << std::endl;

    EXPECT_LT(p99, 50.0);
    EXPECT_GE(start_events.load(), 1);
    EXPECT_GE(end_events.load(), 1);
}
