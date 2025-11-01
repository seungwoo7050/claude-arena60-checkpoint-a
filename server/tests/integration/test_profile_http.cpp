#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "arena60/network/metrics_http_server.h"
#include "arena60/network/profile_http_router.h"
#include "arena60/stats/leaderboard_store.h"
#include "arena60/stats/player_profile_service.h"

namespace http = boost::beast::http;

namespace {
using arena60::MatchResult;
using arena60::PlayerMatchStats;
}  // namespace

http::response<http::string_body> PerformRequest(std::uint16_t port, const std::string& target) {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port));
    boost::beast::tcp_stream stream(io_context);
    stream.connect(endpoints);

    http::request<http::string_body> request{http::verb::get, target, 11};
    request.set(http::field::host, "127.0.0.1");
    request.set(http::field::user_agent, "arena60-tests");
    http::write(stream, request);

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> response;
    http::read(stream, buffer, response);
    boost::system::error_code ec;
    stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    stream.socket().close(ec);
    return response;
}

TEST(ProfileHttpRouterIntegrationTest, ServesMetricsProfilesAndLeaderboard) {
    boost::asio::io_context io_context;
    auto leaderboard = std::make_shared<arena60::InMemoryLeaderboardStore>();
    auto profile_service = std::make_shared<arena60::PlayerProfileService>(leaderboard);

    std::vector<PlayerMatchStats> stats{
        PlayerMatchStats{"match-1", "winner", 5, 5, 1, 0, 100, 10},
        PlayerMatchStats{"match-1", "loser", 4, 2, 0, 1, 40, 100},
    };
    MatchResult result{"match-1", "winner", "loser", std::chrono::system_clock::now(), stats};
    profile_service->RecordMatch(result);

    auto metrics_provider = [profile_service]() { return profile_service->MetricsSnapshot(); };
    auto router = std::make_shared<arena60::ProfileHttpRouter>(metrics_provider, profile_service);
    arena60::MetricsHttpServer::RequestHandler handler =
        [router](const http::request<http::string_body>& request) {
            return router->Handle(request);
        };
    auto server = std::make_shared<arena60::MetricsHttpServer>(io_context, 0, handler);
    server->Start();

    std::thread server_thread([&]() { io_context.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto port = server->Port();
    ASSERT_NE(port, 0);

    auto metrics_response = PerformRequest(port, "/metrics");
    EXPECT_EQ(http::status::ok, metrics_response.result());
    EXPECT_EQ("text/plain; version=0.0.4", metrics_response[http::field::content_type]);
    EXPECT_NE(metrics_response.body().find("player_profiles_total"), std::string::npos);

    auto profile_response = PerformRequest(port, "/profiles/winner");
    EXPECT_EQ(http::status::ok, profile_response.result());
    EXPECT_EQ("application/json", profile_response[http::field::content_type]);
    EXPECT_NE(profile_response.body().find("\"player_id\":\"winner\""), std::string::npos);

    auto missing_response = PerformRequest(port, "/profiles/unknown");
    EXPECT_EQ(http::status::not_found, missing_response.result());

    auto leaderboard_response = PerformRequest(port, "/leaderboard?limit=1");
    EXPECT_EQ(http::status::ok, leaderboard_response.result());
    EXPECT_NE(leaderboard_response.body().find("winner"), std::string::npos);

    server->Stop();
    io_context.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}
