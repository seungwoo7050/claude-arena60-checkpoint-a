#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/http.hpp>
#include <csignal>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>

#include "arena60/core/config.h"
#include "arena60/core/game_loop.h"
#include "arena60/game/game_session.h"
#include "arena60/matchmaking/matchmaker.h"
#include "arena60/matchmaking/match_queue.h"
#include "arena60/network/metrics_http_server.h"
#include "arena60/network/profile_http_router.h"
#include "arena60/network/websocket_server.h"
#include "arena60/stats/leaderboard_store.h"
#include "arena60/stats/player_profile_service.h"
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
    auto match_queue = std::make_shared<InMemoryMatchQueue>();
    auto matchmaker = std::make_shared<Matchmaker>(match_queue);
    auto leaderboard = std::make_shared<InMemoryLeaderboardStore>();
    auto profile_service = std::make_shared<PlayerProfileService>(leaderboard);
    auto server = std::make_shared<WebSocketServer>(io_context, config.port(), session, loop);
    server->SetLifecycleHandlers(
        [&, matchmaker](const std::string& player_id) {
            matchmaker->Enqueue(
                MatchRequest{player_id, 1200, std::chrono::steady_clock::now()});
            if (!storage.RecordSessionEvent(player_id, "start")) {
                std::cerr << "Failed to record session start for " << player_id << std::endl;
            }
        },
        [&, matchmaker](const std::string& player_id) {
            matchmaker->Cancel(player_id);
            if (!storage.RecordSessionEvent(player_id, "end")) {
                std::cerr << "Failed to record session end for " << player_id << std::endl;
            }
        });
    server->SetMatchCompletedCallback(
        [profile_service](const MatchResult& result) { profile_service->RecordMatch(result); });

    auto metrics_provider = [&, server, profile_service]() {
        std::ostringstream oss;
        oss << loop.PrometheusSnapshot();
        oss << server->MetricsSnapshot();
        oss << storage.MetricsSnapshot();
        oss << matchmaker->MetricsSnapshot();
        oss << profile_service->MetricsSnapshot();
        return oss.str();
    };
    auto router = std::make_shared<ProfileHttpRouter>(metrics_provider, profile_service);
    MetricsHttpServer::RequestHandler http_handler =
        [router](const boost::beast::http::request<boost::beast::http::string_body>& request) {
            return router->Handle(request);
        };
    auto metrics_server =
        std::make_shared<MetricsHttpServer>(io_context, config.metrics_port(), std::move(http_handler));

    auto matchmaking_timer = std::make_shared<boost::asio::steady_timer>(io_context);
    std::function<void(const boost::system::error_code&)> matchmaking_tick;
    matchmaking_tick = [matchmaking_timer, matchmaker, &matchmaking_tick](
                            const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }
        if (ec) {
            std::cerr << "matchmaking timer error: " << ec.message() << std::endl;
            return;
        }
        matchmaker->RunMatching(std::chrono::steady_clock::now());
        matchmaker->notification_channel().Drain();
        matchmaking_timer->expires_after(std::chrono::milliseconds(200));
        matchmaking_timer->async_wait(matchmaking_tick);
    };
    matchmaking_timer->expires_after(std::chrono::milliseconds(200));
    matchmaking_timer->async_wait(matchmaking_tick);

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code& /*ec*/, int /*signal*/) {
        std::cout << "Signal received. Shutting down." << std::endl;
        server->Stop();
        metrics_server->Stop();
        loop.Stop();
        matchmaking_timer->cancel();
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
