#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <csignal>
#include <iostream>
#include <memory>

#include "arena60/core/config.h"
#include "arena60/core/game_loop.h"
#include "arena60/game/game_session.h"
#include "arena60/network/websocket_server.h"
#include "arena60/storage/postgres_storage.h"

int main() {
  using namespace arena60;

  const auto config = GameConfig::FromEnv();
  std::cout << "Arena60 Game Server starting on port " << config.port() << std::endl;

  GameSession session(config.tick_rate());
  GameLoop loop(config.tick_rate());
  PostgresStorage storage(config.database_dsn());
  storage.Connect();

  boost::asio::io_context io_context;
  auto server = std::make_shared<WebSocketServer>(io_context, config.port(), session, loop);

  boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](const boost::system::error_code& /*ec*/, int /*signal*/) {
    std::cout << "Signal received. Shutting down." << std::endl;
    server->Stop();
    loop.Stop();
    io_context.stop();
  });

  server->Start();
  loop.Start();

  io_context.run();

  loop.Stop();
  loop.Join();

  std::cout << "Arena60 Game Server stopped" << std::endl;
  return 0;
}
