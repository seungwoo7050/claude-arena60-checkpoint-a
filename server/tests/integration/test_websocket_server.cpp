#include <gtest/gtest.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <chrono>
#include <thread>

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

  const auto start = std::chrono::steady_clock::now();
  ws.write(boost::asio::buffer("input player1 1 1 0 0 0 1.0 0.0"));
  boost::beast::multi_buffer buffer;
  ws.read(buffer);
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  const std::string payload = boost::beast::buffers_to_string(buffer.data());
  EXPECT_TRUE(payload.rfind("state", 0) == 0);

  ws.close(websocket::close_code::normal);

  server->Stop();
  loop.Stop();
  io_context.stop();
  loop.Join();
  server_thread.join();

  EXPECT_LT(elapsed.count(), 20);
}
