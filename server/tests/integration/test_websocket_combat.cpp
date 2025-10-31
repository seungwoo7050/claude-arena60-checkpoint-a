#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <cmath>
#include <sstream>
#include <thread>
#include <vector>

#include "arena60/core/game_loop.h"
#include "arena60/game/game_session.h"
#include "arena60/network/websocket_server.h"

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

namespace {
std::string ReadFrame(websocket::stream<tcp::socket>& ws) {
    boost::beast::multi_buffer buffer;
    ws.read(buffer);
    return boost::beast::buffers_to_string(buffer.data());
}

struct ParsedState {
    std::string player_id;
    double x{0.0};
    double y{0.0};
    double facing{0.0};
    std::uint64_t tick{0};
    double delta{0.0};
    int health{0};
    bool alive{false};
    int shots_fired{0};
    int hits_landed{0};
    int deaths{0};
};

bool ParseStateFrame(const std::string& payload, ParsedState& out) {
    std::istringstream iss(payload);
    std::string type;
    iss >> type;
    if (type != "state") {
        return false;
    }
    int alive = 0;
    iss >> out.player_id >> out.x >> out.y >> out.facing >> out.tick >> out.delta >> out.health >>
        alive >> out.shots_fired >> out.hits_landed >> out.deaths;
    if (!iss) {
        return false;
    }
    out.alive = alive != 0;
    return true;
}
}  // namespace

TEST(WebSocketCombatIntegrationTest, BroadcastsDeathEvents) {
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

    boost::asio::io_context client_io1;
    tcp::resolver resolver1(client_io1);
    auto endpoints1 = resolver1.resolve("127.0.0.1", std::to_string(port));
    websocket::stream<tcp::socket> ws1(client_io1);
    boost::asio::connect(ws1.next_layer(), endpoints1.begin(), endpoints1.end());
    ws1.handshake("127.0.0.1", "/");

    boost::asio::io_context client_io2;
    tcp::resolver resolver2(client_io2);
    auto endpoints2 = resolver2.resolve("127.0.0.1", std::to_string(port));
    websocket::stream<tcp::socket> ws2(client_io2);
    boost::asio::connect(ws2.next_layer(), endpoints2.begin(), endpoints2.end());
    ws2.handshake("127.0.0.1", "/");

    // Register both players with initial inputs.
    ws1.write(boost::asio::buffer("input player1 1 0 0 0 0 1.0 0.0 0"));
    ws2.write(boost::asio::buffer("input player2 1 0 0 0 0 1.0 0.0 0"));

    // Drain initial responses.
    ReadFrame(ws1);
    ReadFrame(ws2);

    // Move player2 into the line of fire.
    arena60::MovementInput move;
    move.sequence = 2;
    move.right = true;
    move.mouse_x = 1.0;
    session.ApplyInput("player2", move, 0.08);  // move ~0.4 meters to the right

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bool death_received = false;
    std::string death_payload;
    std::uint64_t death_tick = 0;
    std::vector<ParsedState> shooter_states;

    for (int shot = 0; shot < 5; ++shot) {
        std::ostringstream frame;
        frame << "input player1 " << (2 + shot) << " 0 0 0 0 1.0 0.0 1";
        ws1.write(boost::asio::buffer(frame.str()));

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!death_received && std::chrono::steady_clock::now() < deadline) {
        const std::string payload = ReadFrame(ws1);
        std::istringstream iss(payload);
        std::string type;
        iss >> type;
        if (type == "death") {
            std::string target;
            iss >> target >> death_tick;
            death_payload = payload;
            if (target == "player2") {
                death_received = true;
            }
        } else if (type == "state") {
            ParsedState parsed;
            if (ParseStateFrame(payload, parsed) && parsed.player_id == "player1") {
                shooter_states.push_back(parsed);
            }
        }
    }

    EXPECT_TRUE(death_received);
    EXPECT_FALSE(death_payload.empty());
    EXPECT_GT(death_tick, 0u);

    const auto target_state = session.GetPlayer("player2");
    EXPECT_EQ(target_state.health, 0);
    EXPECT_FALSE(target_state.is_alive);
    EXPECT_EQ(target_state.deaths, 1);

    ASSERT_FALSE(shooter_states.empty());
    const auto& final_shooter_state = shooter_states.back();
    EXPECT_GE(final_shooter_state.shots_fired, 5);
    EXPECT_GE(final_shooter_state.hits_landed, 5);
    EXPECT_EQ(final_shooter_state.deaths, 0);

    std::vector<double> deltas_ms;
    deltas_ms.reserve(shooter_states.size());
    for (const auto& state : shooter_states) {
        deltas_ms.push_back(state.delta * 1000.0);
    }
    if (!deltas_ms.empty()) {
        std::vector<double> window;
        if (deltas_ms.size() > 5) {
            window.assign(deltas_ms.begin() + 5, deltas_ms.end());
        } else {
            window = deltas_ms;
        }
        std::sort(window.begin(), window.end());
        const auto index = static_cast<std::size_t>(std::ceil(window.size() * 0.99)) - 1;
        const auto clamped_index = std::min(index, window.size() - 1);
        const double p99_ms = window[clamped_index];
        EXPECT_LT(p99_ms, 20.0);
    }

    boost::system::error_code ec;
    ws1.next_layer().shutdown(tcp::socket::shutdown_both, ec);
    ws1.next_layer().close(ec);
    ws2.next_layer().shutdown(tcp::socket::shutdown_both, ec);
    ws2.next_layer().close(ec);

    server->Stop();
    loop.Stop();
    io_context.stop();
    loop.Join();
    server_thread.join();
}

TEST(WebSocketCombatIntegrationTest, AcceptsLegacyInputWithoutFireFlag) {
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
    auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port));
    websocket::stream<tcp::socket> ws(client_io);
    boost::asio::connect(ws.next_layer(), endpoints.begin(), endpoints.end());
    ws.handshake("127.0.0.1", "/");

    auto wait_for_state = [&](std::uint64_t max_attempts) {
        for (std::uint64_t attempt = 0; attempt < max_attempts; ++attempt) {
            const std::string payload = ReadFrame(ws);
            ParsedState parsed;
            if (ParseStateFrame(payload, parsed) && parsed.player_id == "legacy") {
                return parsed;
            }
        }
        return ParsedState{};
    };

    ws.write(boost::asio::buffer("input legacy 1 0 0 0 0 1.0 0.0 0"));
    auto initial_state = wait_for_state(5);
    EXPECT_EQ(initial_state.player_id, "legacy");
    EXPECT_EQ(initial_state.shots_fired, 0);
    EXPECT_EQ(initial_state.hits_landed, 0);

    ws.write(boost::asio::buffer("input legacy 2 0 0 0 1 1.0 0.0"));
    auto moved_state = wait_for_state(5);
    EXPECT_EQ(moved_state.player_id, "legacy");
    EXPECT_GT(moved_state.x, initial_state.x);
    EXPECT_EQ(moved_state.shots_fired, 0);
    EXPECT_EQ(session.ActiveProjectileCount(), 0u);

    ws.write(boost::asio::buffer("input legacy 3 0 0 0 0 1.0 0.0 1"));
    auto fired_state = wait_for_state(5);
    EXPECT_EQ(fired_state.player_id, "legacy");
    EXPECT_GE(fired_state.shots_fired, 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_GE(session.ActiveProjectileCount(), 1u);

    boost::system::error_code ec;
    ws.next_layer().shutdown(tcp::socket::shutdown_both, ec);
    ws.next_layer().close(ec);

    server->Stop();
    loop.Stop();
    io_context.stop();
    loop.Join();
    server_thread.join();
}
