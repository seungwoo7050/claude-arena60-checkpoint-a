#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <csignal>
#include <iostream>
#include <memory>
#include <sstream>

#include "arena60/core/config.h"
#include "arena60/core/game_loop.h"
#include "arena60/game/game_session.h"
#include "arena60/network/metrics_http_server.h"
#include "arena60/network/websocket_server.h"
#include "arena60/storage/postgres_storage.h"

int main() {
    using namespace arena60;

    const auto config = GameConfig::FromEnv();
    std::cout << "Arena60 Game Server starting on port " << config.port() << std::endl;

    GameSession session(config.tick_rate());
    GameLoop loop(config.tick_rate());
    PostgresStorage storage(config.database_dsn());
    if (!storage.Connect()) {
        std::cerr << "Failed to connect to Postgres at startup; continuing in degraded mode."
                  << std::endl;
    }

    boost::asio::io_context io_context;
    auto server = std::make_shared<WebSocketServer>(io_context, config.port(), session, loop);
    server->SetLifecycleHandlers(
        [&](const std::string& player_id) {
            if (!storage.RecordSessionEvent(player_id, "start")) {
                std::cerr << "Failed to record session start for " << player_id << std::endl;
            }
        },
        [&](const std::string& player_id) {
            if (!storage.RecordSessionEvent(player_id, "end")) {
                std::cerr << "Failed to record session end for " << player_id << std::endl;
            }
        });

    auto metrics_provider = [&, server]() {
        std::ostringstream oss;
        oss << loop.PrometheusSnapshot();
        oss << server->MetricsSnapshot();
        oss << storage.MetricsSnapshot();
        return oss.str();
    };
    auto metrics_server =
        std::make_shared<MetricsHttpServer>(io_context, config.metrics_port(), metrics_provider);

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code& /*ec*/, int /*signal*/) {
        std::cout << "Signal received. Shutting down." << std::endl;
        server->Stop();
        metrics_server->Stop();
        loop.Stop();
        io_context.stop();
    });

    server->Start();
    metrics_server->Start();
    std::cout << "Metrics endpoint listening on port " << metrics_server->Port() << std::endl;
    loop.Start();

    io_context.run();

    loop.Stop();
    loop.Join();

    std::cout << "Arena60 Game Server stopped" << std::endl;
    return 0;
}
