# Quickstart 11: Boost.Asio & Beast (비동기 I/O + WebSocket)

## 🎯 목표
- **Boost.Asio**: 비동기 I/O 프로그래밍 마스터
- **io_context**: 이벤트 루프 이해 및 사용
- **비동기 TCP**: async_accept, async_read, async_write
- **Boost.Beast**: WebSocket 서버 구현
- **실전**: 게임 서버용 WebSocket 에코 서버 완성

## 📋 사전준비
- [Quickstart 00](00-setup-linux-macos.md) 완료 (Boost 설치됨)
- [Quickstart 04](04-cpp-for-game-server.md) 완료 (C++ 멀티스레딩)
- [Quickstart 10](10-cmake-build-system.md) 완료 (CMake + Boost 링크)

---

## 🔄 Part 1: 동기 vs 비동기

### 1.1 동기 I/O의 문제

```cpp
// 동기 방식 (Blocking)
while (true) {
    int client_fd = accept(server_fd, ...);  // ← 여기서 블록!
    // 클라이언트 1 처리 (10초 걸림)
    // 클라이언트 2는 10초 동안 대기...
}

// 멀티스레드 해결
while (true) {
    int client_fd = accept(server_fd, ...);
    std::thread([client_fd]() {
        // 클라이언트 처리
    }).detach();
}
// 문제: 스레드 1000개 → 메모리 1GB+, 컨텍스트 스위칭 오버헤드
```

### 1.2 비동기 I/O의 장점

```cpp
// 비동기 방식 (Non-blocking)
io_context.async_accept([](client) {
    // 클라이언트 1 처리 시작
});
io_context.async_accept([](client) {
    // 클라이언트 2 처리 시작
});
// 하나의 스레드가 수천 개 연결 처리!

// 장점:
// - 스레드 1개 → 메모리 효율
// - I/O 대기 중에도 다른 작업 가능
// - 게임 서버: 1000명 동시 접속 가능
```

---

## 🚀 Part 2: Boost.Asio 기초

### 2.1 io_context (이벤트 루프)

```cpp
#include <boost/asio.hpp>
#include <iostream>

using boost::asio::ip::tcp;

int main() {
    // io_context: 모든 비동기 작업의 중심
    boost::asio::io_context io;
    
    // 타이머 예제
    boost::asio::steady_timer timer(io, std::chrono::seconds(3));
    
    timer.async_wait([](boost::system::error_code ec) {
        if (!ec) {
            std::cout << "3 seconds passed!" << std::endl;
        }
    });
    
    std::cout << "Timer started, waiting..." << std::endl;
    
    // 이벤트 루프 실행 (블로킹)
    io.run();
    
    std::cout << "io_context finished" << std::endl;
    
    return 0;
}
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.15)
project(AsioExample)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost 1.70 REQUIRED COMPONENTS system)

add_executable(timer_example timer.cpp)
target_link_libraries(timer_example ${Boost_LIBRARIES})
```

**실행**:
```bash
cmake -B build && cmake --build build
./build/timer_example
# Timer started, waiting...
# (3초 후)
# 3 seconds passed!
# io_context finished
```

### 2.2 비동기 TCP Accept

```cpp
#include <boost/asio.hpp>
#include <iostream>
#include <memory>

using boost::asio::ip::tcp;

class AsyncServer {
private:
    boost::asio::io_context& io_;
    tcp::acceptor acceptor_;

public:
    AsyncServer(boost::asio::io_context& io, int port)
        : io_(io), acceptor_(io, tcp::endpoint(tcp::v4(), port))
    {
        start_accept();
    }

private:
    void start_accept() {
        // 새 소켓 준비
        auto socket = std::make_shared<tcp::socket>(io_);
        
        // 비동기 accept
        acceptor_.async_accept(*socket, [this, socket](boost::system::error_code ec) {
            if (!ec) {
                std::cout << "Client connected!" << std::endl;
                handle_client(socket);
            }
            
            // 다음 연결 대기
            start_accept();
        });
    }
    
    void handle_client(std::shared_ptr<tcp::socket> socket) {
        auto buffer = std::make_shared<std::array<char, 1024>>();
        
        // 비동기 read
        socket->async_read_some(
            boost::asio::buffer(*buffer),
            [socket, buffer](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    std::cout << "Received: " 
                              << std::string(buffer->data(), length) << std::endl;
                    
                    // Echo back (비동기 write)
                    boost::asio::async_write(
                        *socket,
                        boost::asio::buffer(buffer->data(), length),
                        [socket](boost::system::error_code ec, std::size_t) {
                            if (!ec) {
                                std::cout << "Echoed back" << std::endl;
                            }
                        }
                    );
                }
            }
        );
    }
};

int main() {
    try {
        boost::asio::io_context io;
        AsyncServer server(io, 8080);
        
        std::cout << "Async server listening on port 8080" << std::endl;
        
        io.run();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}
```

**테스트**:
```bash
# 터미널 1
./build/async_server

# 터미널 2
echo "Hello Async" | nc localhost 8080
# 응답: Hello Async
```

### 2.3 멀티스레드 io_context

```cpp
#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    boost::asio::io_context io;
    
    // 여러 작업 등록
    for (int i = 0; i < 10; ++i) {
        boost::asio::post(io, [i]() {
            std::cout << "Task " << i 
                      << " on thread " << std::this_thread::get_id() 
                      << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
    }
    
    // 멀티스레드로 io_context 실행
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&io]() {
            io.run();
        });
    }
    
    // 모든 스레드 종료 대기
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
```

---

## 🌐 Part 3: Boost.Beast WebSocket

### 3.1 WebSocket이란?

```
HTTP:
- 요청/응답 모델 (Request → Response)
- 클라이언트가 먼저 요청해야 함
- 실시간 통신 어려움

WebSocket:
- 양방향 통신 (Full-duplex)
- 서버 → 클라이언트 푸시 가능
- 게임 서버에 완벽! (실시간 위치 동기화)
```

### 3.2 WebSocket Echo Server

```cpp
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
private:
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;

public:
    explicit WebSocketSession(tcp::socket socket)
        : ws_(std::move(socket))
    {
    }
    
    void start() {
        // WebSocket 핸드셰이크 수락
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                std::cout << "WebSocket connected" << std::endl;
                self->do_read();
            }
        });
    }

private:
    void do_read() {
        ws_.async_read(
            buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
                if (!ec) {
                    std::cout << "Received: " 
                              << beast::buffers_to_string(self->buffer_.data()) 
                              << std::endl;
                    
                    // Echo back
                    self->ws_.text(self->ws_.got_text());
                    self->do_write();
                }
            }
        );
    }
    
    void do_write() {
        ws_.async_write(
            buffer_.data(),
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (!ec) {
                    self->buffer_.consume(self->buffer_.size());
                    self->do_read();  // 다음 메시지 읽기
                }
            }
        );
    }
};

class WebSocketServer {
private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;

public:
    WebSocketServer(net::io_context& ioc, int port)
        : ioc_(ioc), acceptor_(ioc, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<WebSocketSession>(std::move(socket))->start();
            }
            
            // 다음 연결 대기
            do_accept();
        });
    }
};

int main() {
    try {
        net::io_context ioc;
        WebSocketServer server(ioc, 8080);
        
        std::cout << "WebSocket server listening on port 8080" << std::endl;
        
        ioc.run();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}
```

### 3.3 WebSocket 클라이언트 (HTML)

```html
<!DOCTYPE html>
<html>
<head>
    <title>WebSocket Test</title>
</head>
<body>
    <h1>WebSocket Echo Test</h1>
    <input id="message" type="text" placeholder="Type message">
    <button onclick="send()">Send</button>
    <div id="output"></div>
    
    <script>
        const ws = new WebSocket('ws://localhost:8080');
        
        ws.onopen = () => {
            console.log('Connected');
            document.getElementById('output').innerHTML += '<p>✅ Connected</p>';
        };
        
        ws.onmessage = (event) => {
            console.log('Received:', event.data);
            document.getElementById('output').innerHTML += 
                '<p>📩 ' + event.data + '</p>';
        };
        
        ws.onerror = (error) => {
            console.error('Error:', error);
            document.getElementById('output').innerHTML += '<p>❌ Error</p>';
        };
        
        function send() {
            const msg = document.getElementById('message').value;
            ws.send(msg);
            document.getElementById('output').innerHTML += '<p>📤 ' + msg + '</p>';
            document.getElementById('message').value = '';
        }
    </script>
</body>
</html>
```

**테스트**:
```bash
# 서버 실행
./build/websocket_server

# 브라우저에서 test.html 열기
# 메시지 입력 → Send → Echo 응답 확인
```

---

## 🎮 Part 4: 게임 서버 실전 예제

### 4.1 Player 관리

```cpp
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <set>
#include <mutex>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class GameServer;

class Player : public std::enable_shared_from_this<Player> {
private:
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    GameServer& server_;
    int id_;

public:
    Player(tcp::socket socket, GameServer& server, int id)
        : ws_(std::move(socket)), server_(server), id_(id)
    {
    }
    
    void start() {
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                std::cout << "Player " << self->id_ << " connected" << std::endl;
                self->do_read();
            }
        });
    }
    
    void send(const std::string& message) {
        auto msg = std::make_shared<std::string>(message);
        
        ws_.async_write(
            net::buffer(*msg),
            [self = shared_from_this(), msg](beast::error_code ec, std::size_t) {
                if (ec) {
                    std::cerr << "Send error: " << ec.message() << std::endl;
                }
            }
        );
    }
    
    int get_id() const { return id_; }

private:
    void do_read() {
        ws_.async_read(
            buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t) {
                if (!ec) {
                    std::string msg = beast::buffers_to_string(self->buffer_.data());
                    self->buffer_.consume(self->buffer_.size());
                    
                    std::cout << "Player " << self->id_ << ": " << msg << std::endl;
                    
                    // 모든 플레이어에게 브로드캐스트
                    self->broadcast(msg);
                    
                    self->do_read();
                } else {
                    self->on_disconnect();
                }
            }
        );
    }
    
    void broadcast(const std::string& msg);
    void on_disconnect();
};

class GameServer {
private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::set<std::shared_ptr<Player>> players_;
    std::mutex mutex_;
    int next_id_ = 1;

public:
    GameServer(net::io_context& ioc, int port)
        : ioc_(ioc), acceptor_(ioc, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }
    
    void broadcast(const std::string& message, int sender_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string full_msg = "Player " + std::to_string(sender_id) + ": " + message;
        
        for (auto& player : players_) {
            player->send(full_msg);
        }
    }
    
    void add_player(std::shared_ptr<Player> player) {
        std::lock_guard<std::mutex> lock(mutex_);
        players_.insert(player);
        std::cout << "Total players: " << players_.size() << std::endl;
    }
    
    void remove_player(std::shared_ptr<Player> player) {
        std::lock_guard<std::mutex> lock(mutex_);
        players_.erase(player);
        std::cout << "Player " << player->get_id() << " disconnected. "
                  << "Total: " << players_.size() << std::endl;
    }

private:
    void do_accept() {
        acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto player = std::make_shared<Player>(std::move(socket), *this, next_id_++);
                add_player(player);
                player->start();
            }
            
            do_accept();
        });
    }
};

// Player 메서드 구현
void Player::broadcast(const std::string& msg) {
    server_.broadcast(msg, id_);
}

void Player::on_disconnect() {
    server_.remove_player(shared_from_this());
}

int main() {
    try {
        net::io_context ioc;
        GameServer server(ioc, 8080);
        
        std::cout << "Game server listening on port 8080" << std::endl;
        
        ioc.run();
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}
```

### 4.2 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(GameServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost 찾기
find_package(Boost 1.70 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

# WebSocket Echo Server
add_executable(websocket_server websocket_server.cpp)
target_link_libraries(websocket_server
    ${Boost_LIBRARIES}
    Threads::Threads
)

# Game Server (멀티플레이어)
add_executable(game_server game_server.cpp)
target_link_libraries(game_server
    ${Boost_LIBRARIES}
    Threads::Threads
)
```

---

## 🐛 자주 막히는 부분

### 문제 1: "Boost.Beast not found"
```cmake
# 원인: Boost 버전이 낮음 (Beast는 1.66+)

# 해결:
brew upgrade boost  # macOS
sudo apt install libboost-all-dev  # Linux (최신 버전)

# 버전 확인
brew info boost  # 1.70+ 확인
```

### 문제 2: Segmentation Fault (shared_from_this)
```cpp
// 잘못된 예
class Session {
    void start() {
        // ❌ shared_ptr로 관리 안 되면 crash
        auto self = shared_from_this();
    }
};

Session s;
s.start();  // Crash!

// 올바른 예
auto session = std::make_shared<Session>(...);
session->start();  // ✅ OK
```

### 문제 3: "Operation canceled" 에러
```cpp
// 원인: io_context.run() 전에 객체 소멸

void bad_example() {
    boost::asio::io_context io;
    {
        AsyncServer server(io, 8080);
    }  // server 소멸!
    io.run();  // Operation canceled
}

// 해결:
boost::asio::io_context io;
AsyncServer server(io, 8080);  // 스코프 밖에 선언
io.run();
```

### 문제 4: WebSocket 핸드셰이크 실패
```bash
# 증상: 브라우저에서 연결 안 됨

# 원인: HTTP Upgrade 요청 처리 안 함
# 해결: async_accept() 호출 확인

ws_.async_accept([](beast::error_code ec) {
    if (ec) {
        std::cerr << "Handshake error: " << ec.message() << std::endl;
    }
});
```

### 문제 5: 메모리 누수 (shared_ptr 순환 참조)
```cpp
// 문제: Player ↔ GameServer 순환 참조

class Player {
    std::shared_ptr<GameServer> server_;  // ❌
};

class GameServer {
    std::vector<std::shared_ptr<Player>> players_;  // ❌
};

// 해결: 한쪽은 weak_ptr 또는 참조 사용
class Player {
    GameServer& server_;  // ✅ 참조
};
```

---

## ✅ 완료 체크리스트

### Boost.Asio 기초
- [ ] io_context 이해 (이벤트 루프)
- [ ] async_accept, async_read, async_write 사용
- [ ] 비동기 타이머 동작 확인
- [ ] 멀티스레드 io_context 실행

### Boost.Beast WebSocket
- [ ] WebSocket 서버 구현
- [ ] 브라우저에서 연결 성공
- [ ] Echo 서버 동작 확인
- [ ] HTML 클라이언트 테스트

### 게임 서버 실전
- [ ] Player 클래스 구현
- [ ] 여러 플레이어 동시 연결
- [ ] 브로드캐스트 동작 확인
- [ ] 플레이어 disconnect 처리

### 트러블슈팅
- [ ] shared_from_this 올바른 사용
- [ ] 메모리 누수 방지 (순환 참조)
- [ ] 에러 핸들링 (error_code)

---

## 🚀 다음 단계

✅ Boost.Asio & Beast 완료!

**다음 학습**:
- **Protobuf**: [Quickstart 12: Protobuf Basics](12-protobuf-basics.md) - 데이터 직렬화
- **Database**: [Quickstart 13: PostgreSQL & Redis](13-postgresql-redis-docker.md)

**실전 적용**:
- `mini-gameserver` M1.3 - WebSocket Pong 게임
- M1.4 - 실시간 멀티플레이어

---

## 📚 참고 자료

- [Boost.Asio 공식 문서](https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio.html)
- [Boost.Beast 공식 문서](https://www.boost.org/doc/libs/1_83_0/libs/beast/doc/html/index.html)
- [Boost.Asio Examples](https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio/examples.html)
- [WebSocket Protocol RFC](https://datatracker.ietf.org/doc/html/rfc6455)

---

**Last Updated**: 2025-11-12
