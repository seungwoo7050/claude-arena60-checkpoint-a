#pragma once

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "arena60/core/game_loop.h"
#include "arena60/game/game_session.h"
#include "arena60/stats/match_stats.h"

namespace arena60 {

class WebSocketServer : public std::enable_shared_from_this<WebSocketServer> {
   public:
    WebSocketServer(boost::asio::io_context& io_context, std::uint16_t port, GameSession& session,
                    GameLoop& loop);
    ~WebSocketServer();

    void Start();
    void Stop();

    std::string MetricsSnapshot() const;
    std::uint16_t Port() const;

    void SetLifecycleHandlers(std::function<void(const std::string&)> on_join,
                              std::function<void(const std::string&)> on_leave);
    void SetMatchCompletedCallback(std::function<void(const MatchResult&)> callback);

   private:
    class ClientSession;

    void DoAccept();
    void BroadcastState(std::uint64_t tick, double delta_seconds);
    void RegisterClient(const std::string& player_id, std::shared_ptr<ClientSession> client);
    void UnregisterClient(const std::string& player_id);

    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> running_{false};

    GameSession& session_;
    GameLoop& loop_;

    std::function<void(const std::string&)> on_join_;
    std::function<void(const std::string&)> on_leave_;
    std::function<void(const MatchResult&)> match_completed_callback_;

    mutable std::mutex clients_mutex_;
    std::unordered_map<std::string, std::weak_ptr<ClientSession>> clients_;
    std::uint64_t last_broadcast_tick_{0};
    std::atomic<std::uint32_t> connection_count_{0};

    MatchStatsCollector match_stats_collector_;
};

}  // namespace arena60
