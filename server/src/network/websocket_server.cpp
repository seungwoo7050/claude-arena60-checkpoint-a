#include "arena60/network/websocket_server.h"

#include <atomic>
#include <boost/asio/post.hpp>
#include <iostream>
#include <queue>
#include <sstream>

namespace arena60 {

namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

class WebSocketServer::ClientSession
    : public std::enable_shared_from_this<WebSocketServer::ClientSession> {
   public:
    ClientSession(WebSocketServer& server, tcp::socket socket)
        : server_(server), ws_(std::move(socket)) {}

    void Start() {
        auto self = shared_from_this();
        ws_.set_option(websocket::stream_base::timeout::suggested(boost::beast::role_type::server));
        ws_.async_accept([self](boost::system::error_code ec) {
            if (ec) {
                std::cerr << "websocket accept error: " << ec.message() << std::endl;
                self->Stop();
                return;
            }
            self->ReadLoop();
        });
    }

    void Stop() {
        if (closed_.exchange(true)) {
            return;
        }
        boost::system::error_code ignored;
        ws_.close(websocket::close_code::normal, ignored);
        if (!player_id_.empty()) {
            server_.UnregisterClient(player_id_);
        }
    }

    void EnqueueState(const PlayerState& state, std::uint64_t tick, double delta) {
        auto self = shared_from_this();
        boost::asio::post(ws_.get_executor(), [self, state, tick, delta]() {
            self->DoEnqueueState(state, tick, delta);
        });
    }

    const std::string& player_id() const { return player_id_; }

   private:
    void DoEnqueueState(const PlayerState& state, std::uint64_t tick, double delta) {
        std::ostringstream oss;
        oss << "state " << state.player_id << ' ' << state.x << ' ' << state.y << ' '
            << state.facing_radians << ' ' << tick << ' ' << delta;
        {
            std::lock_guard<std::mutex> lk(write_mutex_);
            write_queue_.push(oss.str());
            if (writing_) {
                return;
            }
            writing_ = true;
        }
        DoWrite();
    }

    void DoWrite() {
        std::string next;
        {
            std::lock_guard<std::mutex> lk(write_mutex_);
            if (write_queue_.empty()) {
                writing_ = false;
                return;
            }
            next = write_queue_.front();
        }

        auto self = shared_from_this();
        ws_.async_write(boost::asio::buffer(next),
                        [self](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
                            self->OnWrite(ec);
                        });
    }

    void OnWrite(boost::system::error_code ec) {
        if (ec) {
            std::cerr << "websocket write error: " << ec.message() << std::endl;
            Stop();
            return;
        }
        {
            std::lock_guard<std::mutex> lk(write_mutex_);
            if (!write_queue_.empty()) {
                write_queue_.pop();
            }
            if (write_queue_.empty()) {
                writing_ = false;
                return;
            }
        }
        DoWrite();
    }

    void ReadLoop() {
        auto self = shared_from_this();
        ws_.async_read(buffer_, [self](boost::system::error_code ec, std::size_t bytes_read) {
            if (ec) {
                if (ec != websocket::error::closed) {
                    std::cerr << "websocket read error: " << ec.message() << std::endl;
                }
                self->Stop();
                return;
            }
            self->OnRead(bytes_read);
        });
    }

    void OnRead(std::size_t /*bytes_read*/) {
        std::string data = boost::beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());

        MovementInput input;
        std::string player_id;
        if (!ParseInputFrame(data, player_id, input)) {
            std::cerr << "invalid input frame: " << data << std::endl;
            ReadLoop();
            return;
        }

        if (player_id_.empty()) {
            player_id_ = player_id;
            server_.RegisterClient(player_id_, shared_from_this());
        }

        session_.ApplyInput(player_id_, input, loop_.TargetDelta());

        ReadLoop();
    }

    bool ParseInputFrame(const std::string& data, std::string& player_id, MovementInput& input) {
        std::istringstream iss(data);
        std::string type;
        iss >> type;
        if (type != "input") {
            return false;
        }
        int up = 0, down = 0, left = 0, right = 0;
        iss >> player_id >> input.sequence >> up >> down >> left >> right >> input.mouse_x >>
            input.mouse_y;
        if (!iss) {
            return false;
        }
        input.up = up != 0;
        input.down = down != 0;
        input.left = left != 0;
        input.right = right != 0;
        return true;
    }

    WebSocketServer& server_;
    GameSession& session_{server_.session_};
    GameLoop& loop_{server_.loop_};
    websocket::stream<tcp::socket> ws_;
    boost::beast::flat_buffer buffer_;
    std::string player_id_;

    std::mutex write_mutex_;
    std::queue<std::string> write_queue_;
    bool writing_{false};
    std::atomic<bool> closed_{false};
};

WebSocketServer::WebSocketServer(boost::asio::io_context& io_context, std::uint16_t port,
                                 GameSession& session, GameLoop& loop)
    : io_context_(io_context),
      acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      session_(session),
      loop_(loop) {}

WebSocketServer::~WebSocketServer() { Stop(); }

void WebSocketServer::Start() {
    if (running_.exchange(true)) {
        return;
    }
    auto self = shared_from_this();
    loop_.SetUpdateCallback([self](const TickInfo& tick) {
        boost::asio::post(self->io_context_,
                          [self, tick]() { self->BroadcastState(tick.tick, tick.delta_seconds); });
    });
    DoAccept();
}

void WebSocketServer::Stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    boost::system::error_code ec;
    acceptor_.close(ec);
    std::vector<std::shared_ptr<ClientSession>> alive;
    {
        std::lock_guard<std::mutex> lk(clients_mutex_);
        for (auto& kv : clients_) {
            if (auto client = kv.second.lock()) {
                alive.push_back(client);
            }
        }
        clients_.clear();
    }
    for (auto& client : alive) {
        client->Stop();
    }
}

std::string WebSocketServer::MetricsSnapshot() const {
    std::ostringstream oss;
    oss << "# TYPE websocket_connections_total gauge\n";
    oss << "websocket_connections_total " << connection_count_.load() << "\n";
    return oss.str();
}

std::uint16_t WebSocketServer::Port() const {
    boost::system::error_code ec;
    auto endpoint = acceptor_.local_endpoint(ec);
    if (ec) {
        return 0;
    }
    return endpoint.port();
}

void WebSocketServer::DoAccept() {
    acceptor_.async_accept(
        [self = shared_from_this()](boost::system::error_code ec, tcp::socket socket) {
            if (ec) {
                std::cerr << "accept error: " << ec.message() << std::endl;
                return;
            }
            auto client = std::make_shared<ClientSession>(*self, std::move(socket));
            client->Start();
            self->DoAccept();
        });
}

void WebSocketServer::BroadcastState(std::uint64_t tick, double delta_seconds) {
    std::vector<std::shared_ptr<ClientSession>> alive;
    {
        std::lock_guard<std::mutex> lk(clients_mutex_);
        for (auto it = clients_.begin(); it != clients_.end();) {
            if (auto client = it->second.lock()) {
                alive.push_back(client);
                ++it;
            } else {
                it = clients_.erase(it);
            }
        }
    }

    for (auto& client : alive) {
        try {
            auto state = session_.GetPlayer(client->player_id());
            client->EnqueueState(state, tick, delta_seconds);
        } catch (const std::exception& ex) {
            std::cerr << "state broadcast failed: " << ex.what() << std::endl;
        }
    }
    last_broadcast_tick_ = tick;
}

void WebSocketServer::RegisterClient(const std::string& player_id,
                                     std::shared_ptr<ClientSession> client) {
    std::shared_ptr<ClientSession> previous;
    {
        std::lock_guard<std::mutex> lk(clients_mutex_);
        auto it = clients_.find(player_id);
        if (it != clients_.end()) {
            previous = it->second.lock();
        }
    }
    if (previous) {
        previous->Stop();
    }
    {
        std::lock_guard<std::mutex> lk(clients_mutex_);
        clients_[player_id] = client;
        connection_count_.fetch_add(1, std::memory_order_relaxed);
    }
    session_.UpsertPlayer(player_id);
    if (on_join_) {
        on_join_(player_id);
    }
}

void WebSocketServer::UnregisterClient(const std::string& player_id) {
    {
        std::lock_guard<std::mutex> lk(clients_mutex_);
        auto it = clients_.find(player_id);
        if (it != clients_.end()) {
            clients_.erase(it);
            auto current = connection_count_.load(std::memory_order_relaxed);
            while (current != 0 && !connection_count_.compare_exchange_weak(
                                       current, current - 1, std::memory_order_relaxed)) {
            }
        }
    }
    session_.RemovePlayer(player_id);
    if (on_leave_) {
        on_leave_(player_id);
    }
}

void WebSocketServer::SetLifecycleHandlers(std::function<void(const std::string&)> on_join,
                                           std::function<void(const std::string&)> on_leave) {
    on_join_ = std::move(on_join);
    on_leave_ = std::move(on_leave);
}

}  // namespace arena60
