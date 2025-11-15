# Quickstart 34: WebSocket Game Server - 멀티플레이어 Pong

**목표**: Boost.Beast를 사용하여 WebSocket 기반 실시간 멀티플레이어 게임 서버를 구축합니다.

**대상**: `mini-gameserver` Phase 1 Milestone 1.3 (WebSocket 멀티플레이어)

**난이도**: ⭐⭐⭐⭐ (Intermediate-Advanced)

**소요 시간**: 90분

**선행 학습**:
- 30-cpp-for-game-server.md (TCP 소켓)
- 32-cpp-game-loop.md (게임 루프)
- 33-boost-asio-beast.md (Boost.Beast 기초)

**학습 목표**:
1. WebSocket 프로토콜 이해 (Handshake, Frame)
2. Boost.Beast를 사용한 WebSocket 서버 구현
3. 멀티플레이어 게임 상태 관리
4. Room 패턴과 브로드캐스트 메시징
5. 실시간 동기화 (60 TPS)

---

## Part 1: WebSocket 기초 (15분)

### 1.1 WebSocket이란?

WebSocket은 클라이언트와 서버 간 **양방향 실시간 통신**을 제공하는 프로토콜입니다.

#### HTTP vs WebSocket

| 특징 | HTTP | WebSocket |
|-----|------|-----------|
| 연결 방식 | Request-Response (단방향) | Full-duplex (양방향) |
| 오버헤드 | 각 요청마다 헤더 | 초기 handshake만 |
| 실시간성 | Polling 필요 | 서버 → 클라이언트 push 가능 |
| 사용 사례 | REST API, 파일 다운로드 | 채팅, 게임, 실시간 대시보드 |

#### WebSocket Handshake

```
Client → Server (HTTP Upgrade 요청):
GET /chat HTTP/1.1
Host: server.example.com
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13

Server → Client (Switching Protocols):
HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=

↓ 이후 WebSocket Frame 통신
```

#### WebSocket Frame 구조

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
```

**주요 필드**:
- `FIN`: 마지막 프레임 여부
- `opcode`: 0x1 (text), 0x2 (binary), 0x8 (close), 0x9 (ping), 0xA (pong)
- `MASK`: 클라이언트→서버는 항상 masked
- `Payload len`: 데이터 길이

**Boost.Beast는 이 모든 복잡성을 추상화**합니다!

---

### 1.2 간단한 Echo WebSocket 서버

```cpp
// websocket_echo.cpp
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// WebSocket 세션 (한 클라이언트 연결)
class session : public std::enable_shared_from_this<session>
{
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;

public:
    explicit session(tcp::socket socket)
        : ws_(std::move(socket))
    {
    }

    void run()
    {
        // WebSocket handshake 수행
        ws_.async_accept(
            beast::bind_front_handler(
                &session::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "accept");

        // 메시지 읽기 시작
        do_read();
    }

    void do_read()
    {
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec == websocket::error::closed)
            return;

        if(ec)
            return fail(ec, "read");

        // Echo back (받은 메시지를 그대로 전송)
        ws_.text(ws_.got_text());
        ws_.async_write(
            buffer_.data(),
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this()));
    }

    void on_write(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        // 버퍼 클리어하고 다음 메시지 대기
        buffer_.consume(buffer_.size());
        do_read();
    }

    void fail(beast::error_code ec, char const* what)
    {
        std::cerr << what << ": " << ec.message() << "\n";
    }
};

// TCP Listener
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;

public:
    listener(net::io_context& ioc, tcp::endpoint endpoint)
        : ioc_(ioc)
        , acceptor_(ioc)
    {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if(ec) { fail(ec, "open"); return; }

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if(ec) { fail(ec, "set_option"); return; }

        acceptor_.bind(endpoint, ec);
        if(ec) { fail(ec, "bind"); return; }

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if(ec) { fail(ec, "listen"); return; }
    }

    void run()
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket)
    {
        if(ec)
            return fail(ec, "accept");

        // 새 세션 생성
        std::make_shared<session>(std::move(socket))->run();

        // 다음 연결 대기
        do_accept();
    }

    void fail(beast::error_code ec, char const* what)
    {
        std::cerr << what << ": " << ec.message() << "\n";
    }
};

int main()
{
    net::io_context ioc{1};
    auto const address = net::ip::make_address("0.0.0.0");
    auto const port = static_cast<unsigned short>(8080);

    std::make_shared<listener>(
        ioc,
        tcp::endpoint{address, port})->run();

    std::cout << "WebSocket Echo Server running on ws://0.0.0.0:8080\n";
    ioc.run();

    return 0;
}
```

#### 빌드 및 실행

```bash
g++ -std=c++17 websocket_echo.cpp -lboost_system -pthread -o ws_echo
./ws_echo
```

#### 웹 브라우저에서 테스트

브라우저 개발자 도구 콘솔에서:

```javascript
const ws = new WebSocket('ws://localhost:8080');

ws.onopen = () => {
    console.log('Connected!');
    ws.send('Hello WebSocket!');
};

ws.onmessage = (event) => {
    console.log('Received:', event.data);
};

ws.onerror = (error) => {
    console.error('WebSocket Error:', error);
};

ws.onclose = () => {
    console.log('Connection closed');
};
```

**실행 결과**:
```
Connected!
Received: Hello WebSocket!
```

---

## Part 2: 멀티플레이어 Pong 서버 설계 (30분)

### 2.1 아키텍처 개요

```
┌─────────────────────────────────────────────────────────┐
│                    Game Server                          │
│                                                         │
│  ┌──────────────┐      ┌──────────────┐               │
│  │   Room 1     │      │   Room 2     │               │
│  │              │      │              │               │
│  │ Player 1     │      │ Player 3     │               │
│  │ Player 2     │      │ Player 4     │               │
│  │              │      │              │               │
│  │ Ball State   │      │ Ball State   │               │
│  │ Score: 0-0   │      │ Score: 3-2   │               │
│  └──────────────┘      └──────────────┘               │
│         ↓                      ↓                       │
│    Game Loop                Game Loop                  │
│    60 TPS                   60 TPS                     │
└─────────────────────────────────────────────────────────┘
         ↓                          ↓
    WebSocket                  WebSocket
         ↓                          ↓
┌─────────────────┐        ┌─────────────────┐
│  Browser Client │        │  Browser Client │
│    (Player 1)   │        │    (Player 3)   │
└─────────────────┘        └─────────────────┘
```

**핵심 컴포넌트**:
1. **Room**: 게임 세션 (2-4명의 플레이어)
2. **Player**: 패들 위치, 점수
3. **Ball**: 위치, 속도
4. **Game Loop**: 고정 타임스텝 (60 TPS)
5. **Broadcast**: Room 내 모든 플레이어에게 상태 전송

---

### 2.2 데이터 구조

```cpp
// game_types.h
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

// 플레이어 정보
struct Player {
    int id;
    std::string name;
    float paddle_y;          // 패들 Y 위치 (0~1)
    int score;
    bool ready;
    std::shared_ptr<websocket::stream<tcp::socket>> ws;

    Player(int id, std::string name, 
           std::shared_ptr<websocket::stream<tcp::socket>> ws)
        : id(id), name(name), paddle_y(0.5f), score(0), ready(false), ws(ws) {}
};

// 게임 공
struct Ball {
    float x, y;              // 위치 (0~1)
    float vx, vy;            // 속도
    
    Ball() : x(0.5f), y(0.5f), vx(0.01f), vy(0.01f) {}
    
    void reset() {
        x = 0.5f;
        y = 0.5f;
        // 랜덤 방향
        vx = (rand() % 2 == 0 ? 1 : -1) * 0.01f;
        vy = (rand() % 2 == 0 ? 1 : -1) * 0.005f;
    }
};

// 게임 상태
enum class GameState {
    WAITING,    // 플레이어 대기 중
    PLAYING,    // 게임 진행 중
    FINISHED    // 게임 종료
};

// 메시지 타입
enum class MessageType {
    JOIN,           // 플레이어 참가
    READY,          // 준비 완료
    INPUT,          // 입력 (패들 이동)
    STATE_UPDATE,   // 게임 상태 업데이트
    SCORE,          // 점수 업데이트
    GAME_OVER       // 게임 종료
};
```

---

### 2.3 Room 클래스

```cpp
// room.h
#pragma once
#include "game_types.h"
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Room : public std::enable_shared_from_this<Room> {
private:
    int room_id_;
    std::vector<std::shared_ptr<Player>> players_;
    Ball ball_;
    GameState state_;
    std::chrono::steady_clock::time_point last_update_;
    
    const float PADDLE_WIDTH = 0.02f;
    const float PADDLE_HEIGHT = 0.15f;
    const float BALL_SIZE = 0.02f;
    const int MAX_SCORE = 5;

public:
    Room(int id) 
        : room_id_(id), state_(GameState::WAITING), 
          last_update_(std::chrono::steady_clock::now()) 
    {
        srand(time(nullptr));
    }

    // 플레이어 추가
    bool add_player(std::shared_ptr<Player> player) {
        if (players_.size() >= 4) {
            return false;  // 방이 가득참
        }
        players_.push_back(player);
        
        // 환영 메시지 전송
        json welcome = {
            {"type", "welcome"},
            {"player_id", player->id},
            {"room_id", room_id_}
        };
        send_to_player(player, welcome.dump());
        
        // 다른 플레이어들에게 알림
        json join_msg = {
            {"type", "player_joined"},
            {"player_id", player->id},
            {"player_name", player->name}
        };
        broadcast(join_msg.dump(), player->id);
        
        return true;
    }

    // 플레이어 제거
    void remove_player(int player_id) {
        auto it = std::remove_if(players_.begin(), players_.end(),
            [player_id](const auto& p) { return p->id == player_id; });
        
        if (it != players_.end()) {
            players_.erase(it, players_.end());
            
            json leave_msg = {
                {"type", "player_left"},
                {"player_id", player_id}
            };
            broadcast(leave_msg.dump());
            
            // 플레이어가 모두 나가면 방 정리
            if (players_.empty()) {
                state_ = GameState::FINISHED;
            }
        }
    }

    // 플레이어 준비 상태 설정
    void set_player_ready(int player_id, bool ready) {
        for (auto& player : players_) {
            if (player->id == player_id) {
                player->ready = ready;
                break;
            }
        }
        
        // 모든 플레이어가 준비되면 게임 시작
        if (all_players_ready() && players_.size() >= 2) {
            start_game();
        }
    }

    // 게임 시작
    void start_game() {
        state_ = GameState::PLAYING;
        ball_.reset();
        
        for (auto& player : players_) {
            player->score = 0;
            player->paddle_y = 0.5f;
        }
        
        json start_msg = {
            {"type", "game_start"}
        };
        broadcast(start_msg.dump());
        
        last_update_ = std::chrono::steady_clock::now();
    }

    // 게임 업데이트 (60 TPS)
    void update(float dt) {
        if (state_ != GameState::PLAYING) return;

        // 공 이동
        ball_.x += ball_.vx;
        ball_.y += ball_.vy;

        // 벽 충돌 (상/하)
        if (ball_.y <= 0.0f || ball_.y >= 1.0f) {
            ball_.vy = -ball_.vy;
            ball_.y = std::clamp(ball_.y, 0.0f, 1.0f);
        }

        // 패들 충돌 체크
        check_paddle_collision();

        // 득점 체크 (좌/우)
        if (ball_.x <= 0.0f) {
            // 오른쪽 플레이어 득점
            if (players_.size() >= 2) {
                players_[1]->score++;
                send_score_update();
            }
            ball_.reset();
        } else if (ball_.x >= 1.0f) {
            // 왼쪽 플레이어 득점
            if (players_.size() >= 1) {
                players_[0]->score++;
                send_score_update();
            }
            ball_.reset();
        }

        // 승리 조건 체크
        for (auto& player : players_) {
            if (player->score >= MAX_SCORE) {
                end_game(player->id);
                return;
            }
        }

        // 상태 브로드캐스트
        broadcast_state();
    }

    // 패들 충돌 체크
    void check_paddle_collision() {
        if (players_.size() < 2) return;

        // 왼쪽 패들 (Player 0)
        if (ball_.x <= PADDLE_WIDTH && ball_.vx < 0) {
            float paddle_top = players_[0]->paddle_y - PADDLE_HEIGHT/2;
            float paddle_bottom = players_[0]->paddle_y + PADDLE_HEIGHT/2;
            
            if (ball_.y >= paddle_top && ball_.y <= paddle_bottom) {
                ball_.vx = -ball_.vx;
                ball_.x = PADDLE_WIDTH;
                
                // 패들 중심으로부터의 거리에 따라 각도 변화
                float hit_pos = (ball_.y - players_[0]->paddle_y) / (PADDLE_HEIGHT/2);
                ball_.vy += hit_pos * 0.01f;
            }
        }

        // 오른쪽 패들 (Player 1)
        if (ball_.x >= 1.0f - PADDLE_WIDTH && ball_.vx > 0) {
            float paddle_top = players_[1]->paddle_y - PADDLE_HEIGHT/2;
            float paddle_bottom = players_[1]->paddle_y + PADDLE_HEIGHT/2;
            
            if (ball_.y >= paddle_top && ball_.y <= paddle_bottom) {
                ball_.vx = -ball_.vx;
                ball_.x = 1.0f - PADDLE_WIDTH;
                
                float hit_pos = (ball_.y - players_[1]->paddle_y) / (PADDLE_HEIGHT/2);
                ball_.vy += hit_pos * 0.01f;
            }
        }
    }

    // 플레이어 입력 처리
    void handle_input(int player_id, const json& input) {
        for (auto& player : players_) {
            if (player->id == player_id) {
                if (input.contains("paddle_y")) {
                    player->paddle_y = std::clamp(
                        input["paddle_y"].get<float>(), 
                        PADDLE_HEIGHT/2, 
                        1.0f - PADDLE_HEIGHT/2
                    );
                }
                break;
            }
        }
    }

    // 게임 상태 브로드캐스트
    void broadcast_state() {
        json state = {
            {"type", "state_update"},
            {"ball", {
                {"x", ball_.x},
                {"y", ball_.y}
            }},
            {"players", json::array()}
        };

        for (auto& player : players_) {
            state["players"].push_back({
                {"id", player->id},
                {"paddle_y", player->paddle_y},
                {"score", player->score}
            });
        }

        broadcast(state.dump());
    }

    // 점수 업데이트 전송
    void send_score_update() {
        json score_msg = {
            {"type", "score_update"},
            {"scores", json::array()}
        };

        for (auto& player : players_) {
            score_msg["scores"].push_back({
                {"id", player->id},
                {"score", player->score}
            });
        }

        broadcast(score_msg.dump());
    }

    // 게임 종료
    void end_game(int winner_id) {
        state_ = GameState::FINISHED;

        json game_over = {
            {"type", "game_over"},
            {"winner_id", winner_id}
        };

        broadcast(game_over.dump());
    }

    // 브로드캐스트 (모든 플레이어에게 전송)
    void broadcast(const std::string& message, int exclude_player_id = -1) {
        for (auto& player : players_) {
            if (player->id != exclude_player_id) {
                send_to_player(player, message);
            }
        }
    }

    // 특정 플레이어에게 전송
    void send_to_player(std::shared_ptr<Player> player, const std::string& message) {
        try {
            player->ws->text(true);
            player->ws->write(boost::asio::buffer(message));
        } catch (const std::exception& e) {
            // 연결이 끊긴 경우 처리
            remove_player(player->id);
        }
    }

    // 모든 플레이어가 준비되었는지 확인
    bool all_players_ready() const {
        if (players_.empty()) return false;
        
        for (const auto& player : players_) {
            if (!player->ready) return false;
        }
        return true;
    }

    // Getter
    int get_room_id() const { return room_id_; }
    GameState get_state() const { return state_; }
    size_t get_player_count() const { return players_.size(); }
    bool is_empty() const { return players_.empty(); }
};
```

---

## Part 3: 완전한 WebSocket Pong 서버 (40분)

### 3.1 WebSocket Session

```cpp
// websocket_session.h
#pragma once
#include "game_types.h"
#include "room.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class RoomManager;  // Forward declaration

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
private:
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    std::shared_ptr<Player> player_;
    std::shared_ptr<Room> room_;
    RoomManager& room_manager_;
    int player_id_;
    
    static std::atomic<int> next_player_id_;

public:
    WebSocketSession(tcp::socket socket, RoomManager& room_manager)
        : ws_(std::move(socket))
        , room_manager_(room_manager)
        , player_id_(next_player_id_++)
    {
    }

    ~WebSocketSession() {
        if (room_) {
            room_->remove_player(player_id_);
        }
    }

    void run() {
        // WebSocket handshake
        ws_.async_accept(
            beast::bind_front_handler(
                &WebSocketSession::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec) {
        if (ec)
            return fail(ec, "accept");

        // 메시지 읽기 시작
        do_read();
    }

    void do_read() {
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &WebSocketSession::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec == websocket::error::closed)
            return;

        if (ec)
            return fail(ec, "read");

        // 메시지 파싱
        std::string message = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());

        try {
            json msg = json::parse(message);
            handle_message(msg);
        } catch (const std::exception& e) {
            std::cerr << "JSON parse error: " << e.what() << "\n";
        }

        // 다음 메시지 읽기
        do_read();
    }

    void handle_message(const json& msg);  // 구현은 room_manager.h 이후

    void fail(beast::error_code ec, char const* what) {
        std::cerr << what << ": " << ec.message() << "\n";
    }
};

std::atomic<int> WebSocketSession::next_player_id_{1};
```

---

### 3.2 Room Manager

```cpp
// room_manager.h
#pragma once
#include "room.h"
#include "websocket_session.h"
#include <mutex>
#include <thread>

class RoomManager {
private:
    std::unordered_map<int, std::shared_ptr<Room>> rooms_;
    std::mutex mutex_;
    int next_room_id_ = 1;
    std::thread game_loop_thread_;
    std::atomic<bool> running_{true};

public:
    RoomManager() {
        // 게임 루프 스레드 시작 (60 TPS)
        game_loop_thread_ = std::thread([this]() {
            const auto frame_duration = std::chrono::microseconds(16667);  // ~60 FPS
            auto last_time = std::chrono::steady_clock::now();

            while (running_) {
                auto current_time = std::chrono::steady_clock::now();
                auto delta = std::chrono::duration_cast<std::chrono::microseconds>(
                    current_time - last_time).count() / 1000000.0f;
                last_time = current_time;

                // 모든 room 업데이트
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    for (auto& [id, room] : rooms_) {
                        room->update(delta);
                    }

                    // 빈 room 정리
                    auto it = rooms_.begin();
                    while (it != rooms_.end()) {
                        if (it->second->is_empty()) {
                            std::cout << "Removing empty room " << it->first << "\n";
                            it = rooms_.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }

                // 60 FPS 유지
                std::this_thread::sleep_until(current_time + frame_duration);
            }
        });
    }

    ~RoomManager() {
        running_ = false;
        if (game_loop_thread_.joinable()) {
            game_loop_thread_.join();
        }
    }

    // Room 생성 또는 참가
    std::shared_ptr<Room> join_or_create_room(std::shared_ptr<Player> player) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 빈 자리가 있는 room 찾기
        for (auto& [id, room] : rooms_) {
            if (room->get_player_count() < 4 && 
                room->get_state() == GameState::WAITING) {
                if (room->add_player(player)) {
                    return room;
                }
            }
        }

        // 새 room 생성
        int room_id = next_room_id_++;
        auto new_room = std::make_shared<Room>(room_id);
        new_room->add_player(player);
        rooms_[room_id] = new_room;

        std::cout << "Created new room " << room_id << "\n";
        return new_room;
    }

    // Room 통계
    void print_stats() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "Active rooms: " << rooms_.size() << "\n";
        for (const auto& [id, room] : rooms_) {
            std::cout << "  Room " << id << ": " 
                      << room->get_player_count() << " players\n";
        }
    }
};

// WebSocketSession::handle_message 구현
void WebSocketSession::handle_message(const json& msg) {
    std::string type = msg["type"];

    if (type == "join") {
        // 플레이어 생성
        std::string name = msg.value("name", "Player");
        player_ = std::make_shared<Player>(
            player_id_, 
            name,
            std::make_shared<websocket::stream<tcp::socket>>(std::move(ws_))
        );

        // Room 참가
        room_ = room_manager_.join_or_create_room(player_);

    } else if (type == "ready") {
        if (room_) {
            room_->set_player_ready(player_id_, true);
        }

    } else if (type == "input") {
        if (room_) {
            room_->handle_input(player_id_, msg);
        }
    }
}
```

---

### 3.3 메인 서버

```cpp
// pong_server.cpp
#include "room_manager.h"
#include <iostream>

class Listener : public std::enable_shared_from_this<Listener> {
    boost::asio::io_context& ioc_;
    tcp::acceptor acceptor_;
    RoomManager& room_manager_;

public:
    Listener(boost::asio::io_context& ioc, tcp::endpoint endpoint, RoomManager& rm)
        : ioc_(ioc), acceptor_(ioc), room_manager_(rm)
    {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec) { fail(ec, "open"); return; }

        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec) { fail(ec, "set_option"); return; }

        acceptor_.bind(endpoint, ec);
        if (ec) { fail(ec, "bind"); return; }

        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec) { fail(ec, "listen"); return; }
    }

    void run() {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            boost::asio::make_strand(ioc_),
            beast::bind_front_handler(
                &Listener::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (ec) {
            fail(ec, "accept");
        } else {
            std::make_shared<WebSocketSession>(
                std::move(socket), 
                room_manager_
            )->run();
        }

        do_accept();
    }

    void fail(beast::error_code ec, char const* what) {
        std::cerr << what << ": " << ec.message() << "\n";
    }
};

int main() {
    try {
        boost::asio::io_context ioc{1};
        RoomManager room_manager;

        auto const address = boost::asio::ip::make_address("0.0.0.0");
        auto const port = static_cast<unsigned short>(8080);

        std::make_shared<Listener>(
            ioc,
            tcp::endpoint{address, port},
            room_manager
        )->run();

        std::cout << "🎮 Pong Server running on ws://0.0.0.0:8080\n";
        std::cout << "Press Ctrl+C to stop\n";

        // 통계 출력 스레드
        std::thread stats_thread([&room_manager]() {
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(10));
                room_manager.print_stats();
            }
        });

        ioc.run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
```

---

## Part 4: HTML 클라이언트 (20분)

### 4.1 pong_client.html

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>WebSocket Pong</title>
    <style>
        body {
            margin: 0;
            padding: 20px;
            font-family: 'Courier New', monospace;
            background: #1a1a1a;
            color: #00ff00;
            display: flex;
            flex-direction: column;
            align-items: center;
        }
        
        h1 {
            text-align: center;
            text-shadow: 0 0 10px #00ff00;
        }
        
        #gameCanvas {
            border: 2px solid #00ff00;
            background: #000;
            box-shadow: 0 0 20px #00ff00;
            margin: 20px 0;
        }
        
        #controls {
            display: flex;
            gap: 10px;
            margin: 20px 0;
        }
        
        button {
            padding: 10px 20px;
            font-size: 16px;
            font-family: 'Courier New', monospace;
            background: #003300;
            color: #00ff00;
            border: 2px solid #00ff00;
            cursor: pointer;
            transition: all 0.3s;
        }
        
        button:hover {
            background: #00ff00;
            color: #000;
        }
        
        button:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        
        #status {
            font-size: 18px;
            margin: 10px 0;
            text-align: center;
        }
        
        #scoreboard {
            font-size: 24px;
            margin: 10px 0;
        }
        
        #log {
            width: 800px;
            height: 150px;
            overflow-y: auto;
            background: #000;
            border: 1px solid #00ff00;
            padding: 10px;
            font-size: 12px;
        }
        
        .log-entry {
            margin: 2px 0;
        }
    </style>
</head>
<body>
    <h1>🏓 WebSocket Pong 🏓</h1>
    
    <div id="status">Not Connected</div>
    <div id="scoreboard">Score: 0 - 0</div>
    
    <canvas id="gameCanvas" width="800" height="600"></canvas>
    
    <div id="controls">
        <button id="connectBtn" onclick="connect()">Connect</button>
        <button id="readyBtn" onclick="ready()" disabled>Ready</button>
    </div>
    
    <div id="log"></div>

    <script>
        const canvas = document.getElementById('gameCanvas');
        const ctx = canvas.getContext('2d');
        const statusDiv = document.getElementById('status');
        const scoreboardDiv = document.getElementById('scoreboard');
        const logDiv = document.getElementById('log');
        const connectBtn = document.getElementById('connectBtn');
        const readyBtn = document.getElementById('readyBtn');
        
        let ws = null;
        let playerId = null;
        let gameState = {
            ball: { x: 0.5, y: 0.5 },
            players: []
        };
        let myPaddleY = 0.5;

        // 로그 출력
        function log(message) {
            const entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.textContent = `[${new Date().toLocaleTimeString()}] ${message}`;
            logDiv.appendChild(entry);
            logDiv.scrollTop = logDiv.scrollHeight;
        }

        // WebSocket 연결
        function connect() {
            ws = new WebSocket('ws://localhost:8080');

            ws.onopen = () => {
                log('✅ Connected to server');
                statusDiv.textContent = 'Connected - Joining room...';
                
                // Join 메시지 전송
                ws.send(JSON.stringify({
                    type: 'join',
                    name: 'Player' + Math.floor(Math.random() * 1000)
                }));
                
                connectBtn.disabled = true;
            };

            ws.onmessage = (event) => {
                const msg = JSON.parse(event.data);
                handleMessage(msg);
            };

            ws.onerror = (error) => {
                log('❌ WebSocket error');
                console.error(error);
            };

            ws.onclose = () => {
                log('🔌 Disconnected');
                statusDiv.textContent = 'Disconnected';
                connectBtn.disabled = false;
                readyBtn.disabled = true;
            };
        }

        // 메시지 처리
        function handleMessage(msg) {
            switch (msg.type) {
                case 'welcome':
                    playerId = msg.player_id;
                    log(`Welcome! You are Player ${playerId}`);
                    statusDiv.textContent = `Waiting for players... (Room ${msg.room_id})`;
                    readyBtn.disabled = false;
                    break;

                case 'player_joined':
                    log(`Player ${msg.player_name} joined`);
                    break;

                case 'player_left':
                    log(`Player ${msg.player_id} left`);
                    break;

                case 'game_start':
                    log('🎮 Game started!');
                    statusDiv.textContent = 'Game in progress';
                    readyBtn.disabled = true;
                    break;

                case 'state_update':
                    gameState = msg;
                    render();
                    break;

                case 'score_update':
                    updateScoreboard(msg.scores);
                    break;

                case 'game_over':
                    log(`🏆 Player ${msg.winner_id} wins!`);
                    statusDiv.textContent = `Game Over - Player ${msg.winner_id} wins!`;
                    readyBtn.disabled = false;
                    readyBtn.textContent = 'Play Again';
                    break;
            }
        }

        // 준비 완료
        function ready() {
            ws.send(JSON.stringify({ type: 'ready' }));
            readyBtn.disabled = true;
            log('✅ Ready!');
        }

        // 점수판 업데이트
        function updateScoreboard(scores) {
            if (scores.length >= 2) {
                scoreboardDiv.textContent = 
                    `Score: ${scores[0].score} - ${scores[1].score}`;
            }
        }

        // 렌더링
        function render() {
            // 배경 클리어
            ctx.fillStyle = '#000';
            ctx.fillRect(0, 0, canvas.width, canvas.height);

            // 중앙선
            ctx.strokeStyle = '#00ff00';
            ctx.setLineDash([5, 5]);
            ctx.beginPath();
            ctx.moveTo(canvas.width / 2, 0);
            ctx.lineTo(canvas.width / 2, canvas.height);
            ctx.stroke();
            ctx.setLineDash([]);

            // 공
            const ballX = gameState.ball.x * canvas.width;
            const ballY = gameState.ball.y * canvas.height;
            ctx.fillStyle = '#00ff00';
            ctx.beginPath();
            ctx.arc(ballX, ballY, 10, 0, Math.PI * 2);
            ctx.fill();

            // 패들
            gameState.players.forEach((player, index) => {
                const paddleWidth = 20;
                const paddleHeight = 100;
                const paddleX = index === 0 ? 10 : canvas.width - 30;
                const paddleY = player.paddle_y * canvas.height - paddleHeight / 2;

                ctx.fillStyle = player.id === playerId ? '#00ff00' : '#ffffff';
                ctx.fillRect(paddleX, paddleY, paddleWidth, paddleHeight);
            });
        }

        // 마우스로 패들 제어
        canvas.addEventListener('mousemove', (e) => {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;

            const rect = canvas.getBoundingClientRect();
            myPaddleY = (e.clientY - rect.top) / canvas.height;
            myPaddleY = Math.max(0.075, Math.min(0.925, myPaddleY));

            ws.send(JSON.stringify({
                type: 'input',
                paddle_y: myPaddleY
            }));
        });

        // 초기 렌더링
        render();
    </script>
</body>
</html>
```

---

## Part 5: 빌드 및 실행 (10분)

### 5.1 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(websocket_pong_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost 찾기
find_package(Boost 1.70 REQUIRED COMPONENTS system)

# nlohmann_json 찾기 (JSON 파싱)
find_package(nlohmann_json 3.2.0 REQUIRED)

# 실행 파일
add_executable(pong_server
    pong_server.cpp
)

target_link_libraries(pong_server
    PRIVATE
        Boost::system
        nlohmann_json::nlohmann_json
        pthread
)

target_include_directories(pong_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 컴파일 옵션
target_compile_options(pong_server PRIVATE
    -Wall -Wextra -Wpedantic
)
```

### 5.2 빌드

```bash
# nlohmann_json 설치 (Ubuntu/Debian)
sudo apt-get install nlohmann-json3-dev

# macOS
brew install nlohmann-json

# 빌드
mkdir build && cd build
cmake ..
cmake --build . -j

# 실행
./pong_server
```

**예상 출력**:
```
🎮 Pong Server running on ws://0.0.0.0:8080
Press Ctrl+C to stop
```

### 5.3 테스트

1. 브라우저에서 `pong_client.html` 열기 (2개 탭)
2. 각 탭에서 "Connect" 클릭
3. 두 탭 모두 "Ready" 클릭
4. 마우스로 패들 움직여서 게임 플레이!

**예상 로그**:
```
[14:23:45] ✅ Connected to server
[14:23:45] Welcome! You are Player 1
[14:23:52] Player Player456 joined
[14:23:55] ✅ Ready!
[14:23:56] 🎮 Game started!
[14:24:10] 🏆 Player 2 wins!
```

---

## Troubleshooting

### 문제 1: "Address already in use"

**증상**:
```
bind: Address already in use
```

**원인**: 8080 포트가 이미 사용 중

**해결**:
```bash
# 프로세스 찾기
lsof -ti:8080

# 종료
lsof -ti:8080 | xargs kill -9

# 또는 다른 포트 사용
# pong_server.cpp에서 port 변경
auto const port = static_cast<unsigned short>(9090);
```

---

### 문제 2: "nlohmann/json.hpp: No such file or directory"

**증상**:
```
fatal error: nlohmann/json.hpp: No such file or directory
```

**원인**: nlohmann_json 라이브러리 미설치

**해결**:
```bash
# Ubuntu/Debian
sudo apt-get install nlohmann-json3-dev

# macOS
brew install nlohmann-json

# 또는 헤더 단일 파일 다운로드
cd your_project_dir
mkdir -p include/nlohmann
wget https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp \
     -O include/nlohmann/json.hpp

# CMakeLists.txt에 추가
target_include_directories(pong_server PRIVATE include)
```

---

### 문제 3: WebSocket 연결이 즉시 끊김

**증상**:
브라우저에서 연결 후 즉시 "Disconnected"

**원인**: 
1. 서버가 실행 중이 아님
2. 방화벽 차단
3. WebSocket handshake 실패

**해결**:
```bash
# 1. 서버 실행 확인
ps aux | grep pong_server

# 2. 포트 리스닝 확인
netstat -an | grep 8080

# 3. 방화벽 규칙 추가 (Ubuntu)
sudo ufw allow 8080/tcp

# 4. 서버 로그 확인 (디버그 출력 추가)
# pong_server.cpp의 on_accept에 추가:
std::cout << "New connection from " 
          << socket.remote_endpoint() << "\n";
```

---

### 문제 4: 게임이 끊기거나 렉이 심함

**증상**:
패들이 부드럽게 움직이지 않음, 공이 순간이동

**원인**:
1. 클라이언트 입력 전송 빈도가 너무 높음
2. 서버 게임 루프가 60 TPS를 유지하지 못함

**해결**:
```javascript
// pong_client.html에서 입력 쓰로틀링 추가
let lastInputTime = 0;
const INPUT_THROTTLE = 16;  // ~60 FPS

canvas.addEventListener('mousemove', (e) => {
    const now = Date.now();
    if (now - lastInputTime < INPUT_THROTTLE) return;
    lastInputTime = now;

    // ... 기존 코드
});
```

```cpp
// room.h에서 프레임 타임 모니터링
void update(float dt) {
    if (dt > 0.05f) {  // 50ms 초과
        std::cerr << "Warning: Frame took " << (dt * 1000) << "ms\n";
    }
    // ... 기존 코드
}
```

---

### 문제 5: 플레이어가 나간 후 room이 정리되지 않음

**증상**:
`Active rooms` 숫자가 계속 증가

**원인**:
WebSocketSession 소멸자가 호출되지 않음

**해결**:
```cpp
// room_manager.h의 게임 루프에서 명시적 정리
while (running_) {
    // ... 업데이트 코드

    // 빈 room 정리 (이미 있음)
    auto it = rooms_.begin();
    while (it != rooms_.end()) {
        if (it->second->is_empty() || 
            it->second->get_state() == GameState::FINISHED) {
            std::cout << "Removing room " << it->first << "\n";
            it = rooms_.erase(it);
        } else {
            ++it;
        }
    }

    std::this_thread::sleep_until(current_time + frame_duration);
}
```

---

## 요약

이번 Quickstart에서 학습한 내용:

1. **WebSocket 프로토콜**: Handshake, Frame 구조
2. **Boost.Beast WebSocket API**: async_accept, async_read, async_write
3. **멀티플레이어 아키텍처**: Room, Player, Broadcast
4. **실시간 게임 루프**: 60 TPS 고정 타임스텝
5. **상태 동기화**: JSON 기반 상태 브로드캐스트

**mini-gameserver Milestone 1.3 완료!** ✅

**다음 단계**:
- 43-jwt-game-integration.md: JWT 인증 통합
- 44-elo-db-integration.md: ELO 랭킹 + PostgreSQL 연동
- 45-matchmaking-system.md: 매치메이킹 큐

**주요 개념**:
- WebSocket은 HTTP 위에서 양방향 실시간 통신 제공
- Boost.Beast는 WebSocket을 C++에서 쉽게 사용 가능하게 함
- Room 패턴으로 멀티플레이어 게임 상태 관리
- 고정 타임스텝 게임 루프로 일관된 시뮬레이션
- Broadcast로 모든 플레이어에게 상태 동기화

이제 WebSocket 기반 실시간 멀티플레이어 게임 서버를 만들 수 있습니다! 🎮
