# Quickstart 43: JWT Game Server Integration - 게임 서버 인증

**목표**: JWT 토큰을 게임 서버에 통합하여 인증된 플레이어만 게임에 참가할 수 있도록 합니다.

**대상**: `mini-gameserver` Phase 1 Milestone 1.7 (JWT 인증 통합)

**난이도**: ⭐⭐⭐⭐ (Advanced)

**소요 시간**: 80분

**선행 학습**:
- 34-websocket-game-server.md (WebSocket 게임 서버)
- 51-jwt-authentication.md (JWT 기초)
- 60-postgresql-redis-docker.md (PostgreSQL, Redis)

**학습 목표**:
1. JWT 토큰을 WebSocket 연결 시 검증
2. 사용자 세션 관리 (Redis)
3. 게임 서버에서 사용자 정보 조회
4. 토큰 갱신 (Refresh Token)
5. 보안 모범 사례

---

## Part 1: JWT 검증 라이브러리 통합 (15분)

### 1.1 jwt-cpp 라이브러리

C++에서 JWT를 사용하기 위해 [jwt-cpp](https://github.com/Thalhammer/jwt-cpp) 라이브러리를 사용합니다.

#### 설치

```bash
# Ubuntu/Debian
sudo apt-get install libjwt-dev libssl-dev

# macOS
brew install jwt-cpp openssl

# 또는 헤더 전용 라이브러리 다운로드
cd your_project
mkdir -p include/jwt-cpp
wget https://raw.githubusercontent.com/Thalhammer/jwt-cpp/master/include/jwt-cpp/jwt.h \
     -O include/jwt-cpp/jwt.h
```

#### CMakeLists.txt 설정

```cmake
cmake_minimum_required(VERSION 3.20)
project(jwt_game_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.70 REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

# jwt-cpp (헤더 전용)
add_library(jwt-cpp INTERFACE)
target_include_directories(jwt-cpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(game_server
    main.cpp
    jwt_validator.cpp
)

target_link_libraries(game_server
    PRIVATE
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        jwt-cpp
        pthread
)
```

---

### 1.2 간단한 JWT 검증 예제

```cpp
// jwt_simple_example.cpp
#include <jwt-cpp/jwt.h>
#include <iostream>
#include <string>

int main() {
    // 비밀 키 (실제로는 환경 변수나 설정 파일에서 읽어야 함)
    std::string secret = "your-256-bit-secret";

    // 1. JWT 토큰 생성
    auto token = jwt::create()
        .set_issuer("game-server")
        .set_type("JWT")
        .set_subject("user123")
        .set_issued_at(std::chrono::system_clock::now())
        .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{1})
        .set_payload_claim("user_id", jwt::claim(std::string("123")))
        .set_payload_claim("username", jwt::claim(std::string("player1")))
        .sign(jwt::algorithm::hs256{secret});

    std::cout << "Generated Token:\n" << token << "\n\n";

    // 2. JWT 토큰 검증
    try {
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret})
            .with_issuer("game-server");

        auto decoded = jwt::decode(token);
        verifier.verify(decoded);

        std::cout << "✅ Token is valid!\n";
        std::cout << "User ID: " << decoded.get_payload_claim("user_id").as_string() << "\n";
        std::cout << "Username: " << decoded.get_payload_claim("username").as_string() << "\n";
        std::cout << "Expires at: " 
                  << std::chrono::system_clock::to_time_t(decoded.get_expires_at()) 
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "❌ Token validation failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

#### 빌드 및 실행

```bash
g++ -std=c++17 jwt_simple_example.cpp -lssl -lcrypto -o jwt_example
./jwt_example
```

**출력**:
```
Generated Token:
eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnYW1lLXNlcnZlciIsInN1YiI6InVzZXIxMjMiLCJpYXQiOjE2OTk5OTk5OTksImV4cCI6MTcwMDAwMzU5OSwidXNlcl9pZCI6IjEyMyIsInVzZXJuYW1lIjoicGxheWVyMSJ9.xxx

✅ Token is valid!
User ID: 123
Username: player1
Expires at: 1700003599
```

---

## Part 2: JWT Validator 클래스 설계 (20분)

### 2.1 JWTValidator 인터페이스

```cpp
// jwt_validator.h
#pragma once
#include <string>
#include <optional>
#include <memory>
#include <jwt-cpp/jwt.h>

struct UserClaims {
    std::string user_id;
    std::string username;
    std::string email;
    std::chrono::system_clock::time_point expires_at;
    
    bool is_expired() const {
        return std::chrono::system_clock::now() > expires_at;
    }
};

class JWTValidator {
private:
    std::string secret_;
    jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson> verifier_;

public:
    explicit JWTValidator(const std::string& secret);
    
    // 토큰 검증 및 클레임 추출
    std::optional<UserClaims> validate(const std::string& token);
    
    // 토큰 생성 (테스트용)
    std::string generate_token(
        const std::string& user_id,
        const std::string& username,
        const std::string& email,
        std::chrono::seconds expires_in = std::chrono::hours{1}
    );
    
    // Refresh 토큰 검증
    bool validate_refresh_token(const std::string& token);
};
```

---

### 2.2 JWTValidator 구현

```cpp
// jwt_validator.cpp
#include "jwt_validator.h"
#include <iostream>

JWTValidator::JWTValidator(const std::string& secret)
    : secret_(secret)
    , verifier_(jwt::verify()
        .allow_algorithm(jwt::algorithm::hs256{secret})
        .with_issuer("game-server"))
{
}

std::optional<UserClaims> JWTValidator::validate(const std::string& token) {
    try {
        // 토큰 디코딩
        auto decoded = jwt::decode(token);
        
        // 서명 및 클레임 검증
        verifier_.verify(decoded);
        
        // 만료 시간 체크
        auto exp = decoded.get_expires_at();
        if (std::chrono::system_clock::now() > exp) {
            std::cerr << "Token expired\n";
            return std::nullopt;
        }
        
        // 클레임 추출
        UserClaims claims;
        claims.user_id = decoded.get_payload_claim("user_id").as_string();
        claims.username = decoded.get_payload_claim("username").as_string();
        claims.email = decoded.get_payload_claim("email").as_string();
        claims.expires_at = exp;
        
        return claims;
        
    } catch (const jwt::error::token_verification_exception& e) {
        std::cerr << "Token verification failed: " << e.what() << "\n";
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Token parsing error: " << e.what() << "\n";
        return std::nullopt;
    }
}

std::string JWTValidator::generate_token(
    const std::string& user_id,
    const std::string& username,
    const std::string& email,
    std::chrono::seconds expires_in
) {
    auto now = std::chrono::system_clock::now();
    
    return jwt::create()
        .set_issuer("game-server")
        .set_type("JWT")
        .set_subject(user_id)
        .set_issued_at(now)
        .set_expires_at(now + expires_in)
        .set_payload_claim("user_id", jwt::claim(user_id))
        .set_payload_claim("username", jwt::claim(username))
        .set_payload_claim("email", jwt::claim(email))
        .sign(jwt::algorithm::hs256{secret_});
}

bool JWTValidator::validate_refresh_token(const std::string& token) {
    try {
        auto decoded = jwt::decode(token);
        
        // Refresh 토큰은 더 긴 만료 시간을 가짐
        auto exp = decoded.get_expires_at();
        if (std::chrono::system_clock::now() > exp) {
            return false;
        }
        
        // 토큰 타입 확인
        auto type_claim = decoded.get_payload_claim("type");
        if (type_claim.as_string() != "refresh") {
            return false;
        }
        
        verifier_.verify(decoded);
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}
```

---

### 2.3 단위 테스트

```cpp
// jwt_validator_test.cpp
#include "jwt_validator.h"
#include <cassert>
#include <iostream>

void test_valid_token() {
    JWTValidator validator("test-secret-key");
    
    // 토큰 생성
    auto token = validator.generate_token("123", "testuser", "test@example.com");
    
    // 검증
    auto claims = validator.validate(token);
    assert(claims.has_value());
    assert(claims->user_id == "123");
    assert(claims->username == "testuser");
    
    std::cout << "✅ test_valid_token passed\n";
}

void test_invalid_token() {
    JWTValidator validator("test-secret-key");
    
    // 잘못된 토큰
    auto claims = validator.validate("invalid.token.here");
    assert(!claims.has_value());
    
    std::cout << "✅ test_invalid_token passed\n";
}

void test_expired_token() {
    JWTValidator validator("test-secret-key");
    
    // 이미 만료된 토큰 생성 (0초 만료)
    auto token = validator.generate_token(
        "123", "testuser", "test@example.com", 
        std::chrono::seconds{0}
    );
    
    // 1초 대기
    std::this_thread::sleep_for(std::chrono::seconds{1});
    
    // 검증 (실패해야 함)
    auto claims = validator.validate(token);
    assert(!claims.has_value());
    
    std::cout << "✅ test_expired_token passed\n";
}

void test_tampered_token() {
    JWTValidator validator("test-secret-key");
    
    auto token = validator.generate_token("123", "testuser", "test@example.com");
    
    // 토큰 변조 (마지막 문자 변경)
    token[token.size() - 1] = 'X';
    
    auto claims = validator.validate(token);
    assert(!claims.has_value());
    
    std::cout << "✅ test_tampered_token passed\n";
}

int main() {
    test_valid_token();
    test_invalid_token();
    test_expired_token();
    test_tampered_token();
    
    std::cout << "\n🎉 All tests passed!\n";
    return 0;
}
```

---

## Part 3: WebSocket 게임 서버에 JWT 통합 (30분)

### 3.1 인증된 WebSocket Session

```cpp
// authenticated_session.h
#pragma once
#include "jwt_validator.h"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <optional>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

class AuthenticatedSession : public std::enable_shared_from_this<AuthenticatedSession> {
private:
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    std::shared_ptr<JWTValidator> jwt_validator_;
    std::optional<UserClaims> user_claims_;
    
    enum class State {
        WAITING_AUTH,    // 인증 대기 중
        AUTHENTICATED,   // 인증 완료
        IN_GAME          // 게임 중
    };
    State state_ = State::WAITING_AUTH;

public:
    AuthenticatedSession(
        tcp::socket socket,
        std::shared_ptr<JWTValidator> jwt_validator
    )
        : ws_(std::move(socket))
        , jwt_validator_(jwt_validator)
    {
    }

    void run() {
        // WebSocket handshake
        ws_.async_accept(
            beast::bind_front_handler(
                &AuthenticatedSession::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec) {
        if (ec)
            return fail(ec, "accept");

        // 인증 대기 상태로 시작
        send_message(R"({"type":"auth_required","message":"Send JWT token"})");
        do_read();
    }

    void do_read() {
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &AuthenticatedSession::on_read,
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
            auto msg = nlohmann::json::parse(message);
            handle_message(msg);
        } catch (const std::exception& e) {
            send_error("Invalid JSON: " + std::string(e.what()));
        }

        do_read();
    }

    void handle_message(const nlohmann::json& msg) {
        std::string type = msg["type"];

        if (state_ == State::WAITING_AUTH) {
            // 인증 메시지만 허용
            if (type == "auth") {
                handle_auth(msg);
            } else {
                send_error("Authentication required");
            }
        } else {
            // 인증 완료 후 메시지 처리
            if (type == "join_game") {
                handle_join_game(msg);
            } else if (type == "game_input") {
                handle_game_input(msg);
            }
        }
    }

    void handle_auth(const nlohmann::json& msg) {
        if (!msg.contains("token")) {
            send_error("Token missing");
            return;
        }

        std::string token = msg["token"];
        auto claims = jwt_validator_->validate(token);

        if (!claims) {
            send_error("Invalid or expired token");
            ws_.close(websocket::close_code::policy_error);
            return;
        }

        // 인증 성공
        user_claims_ = claims;
        state_ = State::AUTHENTICATED;

        nlohmann::json response = {
            {"type", "auth_success"},
            {"user_id", claims->user_id},
            {"username", claims->username}
        };
        send_message(response.dump());

        std::cout << "✅ User authenticated: " << claims->username 
                  << " (ID: " << claims->user_id << ")\n";
    }

    void handle_join_game(const nlohmann::json& msg) {
        if (!user_claims_) {
            send_error("Not authenticated");
            return;
        }

        state_ = State::IN_GAME;
        
        nlohmann::json response = {
            {"type", "game_joined"},
            {"player_id", user_claims_->user_id}
        };
        send_message(response.dump());

        std::cout << "🎮 " << user_claims_->username << " joined game\n";
    }

    void handle_game_input(const nlohmann::json& msg) {
        if (state_ != State::IN_GAME) {
            send_error("Not in game");
            return;
        }

        // 게임 입력 처리
        // ... (기존 게임 로직)
    }

    void send_message(const std::string& message) {
        ws_.text(true);
        ws_.write(boost::asio::buffer(message));
    }

    void send_error(const std::string& error) {
        nlohmann::json response = {
            {"type", "error"},
            {"message", error}
        };
        send_message(response.dump());
    }

    void fail(beast::error_code ec, char const* what) {
        std::cerr << what << ": " << ec.message() << "\n";
    }

    // Getter
    const std::optional<UserClaims>& get_user_claims() const {
        return user_claims_;
    }

    bool is_authenticated() const {
        return user_claims_.has_value();
    }
};
```

---

### 3.2 인증 서버 (간단한 로그인 API)

```cpp
// auth_server.cpp
// HTTP 서버로 JWT 토큰 발급
#include "jwt_validator.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <unordered_map>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using json = nlohmann::json;

// 간단한 사용자 데이터베이스 (실제로는 PostgreSQL 사용)
struct User {
    std::string id;
    std::string username;
    std::string email;
    std::string password_hash;  // bcrypt hash
};

std::unordered_map<std::string, User> users = {
    {"testuser", {"123", "testuser", "test@example.com", "hashed_password"}},
    {"player1", {"456", "player1", "player1@example.com", "hashed_password"}},
};

class AuthSession : public std::enable_shared_from_this<AuthSession> {
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    std::shared_ptr<JWTValidator> jwt_validator_;

public:
    AuthSession(tcp::socket socket, std::shared_ptr<JWTValidator> jwt_validator)
        : socket_(std::move(socket))
        , jwt_validator_(jwt_validator)
    {
    }

    void run() {
        do_read();
    }

    void do_read() {
        http::async_read(
            socket_,
            buffer_,
            request_,
            beast::bind_front_handler(
                &AuthSession::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t) {
        if (ec == http::error::end_of_stream) {
            socket_.shutdown(tcp::socket::shutdown_send, ec);
            return;
        }

        if (ec)
            return fail(ec, "read");

        handle_request();
    }

    void handle_request() {
        // CORS 헤더
        auto add_cors = [](auto& response) {
            response.set(http::field::access_control_allow_origin, "*");
            response.set(http::field::access_control_allow_methods, "POST, OPTIONS");
            response.set(http::field::access_control_allow_headers, "Content-Type");
        };

        // OPTIONS 요청 (CORS preflight)
        if (request_.method() == http::verb::options) {
            http::response<http::empty_body> res{http::status::ok, request_.version()};
            add_cors(res);
            res.prepare_payload();
            return send_response(std::move(res));
        }

        // POST /login
        if (request_.method() == http::verb::post && request_.target() == "/login") {
            return handle_login();
        }

        // POST /refresh
        if (request_.method() == http::verb::post && request_.target() == "/refresh") {
            return handle_refresh();
        }

        // 404
        http::response<http::string_body> res{http::status::not_found, request_.version()};
        res.set(http::field::content_type, "application/json");
        add_cors(res);
        res.body() = R"({"error":"Not found"})";
        res.prepare_payload();
        send_response(std::move(res));
    }

    void handle_login() {
        try {
            auto body = json::parse(request_.body());
            std::string username = body["username"];
            std::string password = body["password"];

            // 사용자 조회
            auto it = users.find(username);
            if (it == users.end()) {
                return send_error(http::status::unauthorized, "Invalid credentials");
            }

            // 비밀번호 검증 (실제로는 bcrypt 사용)
            // if (!verify_bcrypt(password, it->second.password_hash)) { ... }

            // JWT 토큰 생성
            auto& user = it->second;
            auto access_token = jwt_validator_->generate_token(
                user.id, user.username, user.email,
                std::chrono::hours{1}  // 1시간
            );

            // Refresh 토큰 생성 (더 긴 만료 시간)
            auto refresh_token = jwt::create()
                .set_issuer("game-server")
                .set_type("JWT")
                .set_subject(user.id)
                .set_issued_at(std::chrono::system_clock::now())
                .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{24 * 7})
                .set_payload_claim("type", jwt::claim(std::string("refresh")))
                .set_payload_claim("user_id", jwt::claim(user.id))
                .sign(jwt::algorithm::hs256{"your-256-bit-secret"});

            json response = {
                {"access_token", access_token},
                {"refresh_token", refresh_token},
                {"token_type", "Bearer"},
                {"expires_in", 3600},
                {"user", {
                    {"id", user.id},
                    {"username", user.username},
                    {"email", user.email}
                }}
            };

            send_json(http::status::ok, response);

            std::cout << "✅ Login successful: " << username << "\n";

        } catch (const std::exception& e) {
            send_error(http::status::bad_request, e.what());
        }
    }

    void handle_refresh() {
        try {
            auto body = json::parse(request_.body());
            std::string refresh_token = body["refresh_token"];

            if (!jwt_validator_->validate_refresh_token(refresh_token)) {
                return send_error(http::status::unauthorized, "Invalid refresh token");
            }

            auto decoded = jwt::decode(refresh_token);
            std::string user_id = decoded.get_payload_claim("user_id").as_string();

            // 새 access token 발급
            // (실제로는 user_id로 DB에서 사용자 정보 조회)
            auto new_access_token = jwt_validator_->generate_token(
                user_id, "username", "email@example.com",
                std::chrono::hours{1}
            );

            json response = {
                {"access_token", new_access_token},
                {"token_type", "Bearer"},
                {"expires_in", 3600}
            };

            send_json(http::status::ok, response);

        } catch (const std::exception& e) {
            send_error(http::status::bad_request, e.what());
        }
    }

    void send_json(http::status status, const json& body) {
        http::response<http::string_body> res{status, request_.version()};
        res.set(http::field::content_type, "application/json");
        res.set(http::field::access_control_allow_origin, "*");
        res.body() = body.dump();
        res.prepare_payload();
        send_response(std::move(res));
    }

    void send_error(http::status status, const std::string& message) {
        json error = {{"error", message}};
        send_json(status, error);
    }

    template<typename Response>
    void send_response(Response&& res) {
        auto sp = std::make_shared<Response>(std::move(res));
        http::async_write(
            socket_,
            *sp,
            [self = shared_from_this(), sp](beast::error_code ec, std::size_t) {
                self->socket_.shutdown(tcp::socket::shutdown_send, ec);
            });
    }

    void fail(beast::error_code ec, char const* what) {
        std::cerr << what << ": " << ec.message() << "\n";
    }
};

class AuthServer {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<JWTValidator> jwt_validator_;

public:
    AuthServer(net::io_context& ioc, tcp::endpoint endpoint)
        : ioc_(ioc)
        , acceptor_(ioc)
        , jwt_validator_(std::make_shared<JWTValidator>("your-256-bit-secret"))
    {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
    }

    void run() {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            net::make_strand(ioc_),
            [this](beast::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<AuthSession>(
                        std::move(socket),
                        jwt_validator_
                    )->run();
                }
                do_accept();
            });
    }
};

int main() {
    net::io_context ioc{1};
    AuthServer server(ioc, tcp::endpoint{net::ip::make_address("0.0.0.0"), 8081});
    server.run();

    std::cout << "🔐 Auth Server running on http://0.0.0.0:8081\n";
    std::cout << "Endpoints:\n";
    std::cout << "  POST /login - Login and get JWT token\n";
    std::cout << "  POST /refresh - Refresh access token\n";

    ioc.run();
    return 0;
}
```

---

## Part 4: 클라이언트 통합 (15분)

### 4.1 HTML 클라이언트 (JWT 인증)

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Authenticated Pong</title>
    <style>
        body {
            margin: 0;
            padding: 20px;
            font-family: 'Courier New', monospace;
            background: #1a1a1a;
            color: #00ff00;
        }
        
        .container {
            max-width: 800px;
            margin: 0 auto;
        }
        
        #loginForm, #gameArea {
            background: #000;
            border: 2px solid #00ff00;
            padding: 20px;
            margin: 20px 0;
        }
        
        input {
            width: 100%;
            padding: 10px;
            margin: 10px 0;
            background: #003300;
            border: 1px solid #00ff00;
            color: #00ff00;
            font-family: 'Courier New', monospace;
        }
        
        button {
            padding: 10px 20px;
            background: #003300;
            color: #00ff00;
            border: 2px solid #00ff00;
            cursor: pointer;
            font-family: 'Courier New', monospace;
        }
        
        button:hover {
            background: #00ff00;
            color: #000;
        }
        
        #status {
            margin: 10px 0;
            padding: 10px;
            background: #003300;
        }
        
        #gameArea {
            display: none;
        }
        
        canvas {
            border: 2px solid #00ff00;
            margin: 20px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🏓 Authenticated Pong 🔐</h1>
        
        <div id="status">Status: Not logged in</div>
        
        <!-- 로그인 폼 -->
        <div id="loginForm">
            <h2>Login</h2>
            <input type="text" id="username" placeholder="Username" value="testuser">
            <input type="password" id="password" placeholder="Password" value="password">
            <button onclick="login()">Login</button>
        </div>
        
        <!-- 게임 영역 -->
        <div id="gameArea">
            <h2>Welcome, <span id="playerName"></span>!</h2>
            <button onclick="joinGame()">Join Game</button>
            <canvas id="gameCanvas" width="800" height="600"></canvas>
            <button onclick="logout()">Logout</button>
        </div>
    </div>

    <script>
        let accessToken = null;
        let refreshToken = null;
        let ws = null;
        let userId = null;
        
        const statusDiv = document.getElementById('status');
        const loginForm = document.getElementById('loginForm');
        const gameArea = document.getElementById('gameArea');
        const playerNameSpan = document.getElementById('playerName');

        function updateStatus(message) {
            statusDiv.textContent = 'Status: ' + message;
            console.log(message);
        }

        async function login() {
            const username = document.getElementById('username').value;
            const password = document.getElementById('password').value;

            updateStatus('Logging in...');

            try {
                const response = await fetch('http://localhost:8081/login', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({ username, password })
                });

                const data = await response.json();

                if (response.ok) {
                    accessToken = data.access_token;
                    refreshToken = data.refresh_token;
                    userId = data.user.id;
                    
                    // 로컬 스토리지에 저장
                    localStorage.setItem('access_token', accessToken);
                    localStorage.setItem('refresh_token', refreshToken);
                    
                    updateStatus('✅ Logged in as ' + data.user.username);
                    playerNameSpan.textContent = data.user.username;
                    
                    // UI 전환
                    loginForm.style.display = 'none';
                    gameArea.style.display = 'block';
                    
                    // WebSocket 연결
                    connectWebSocket();
                } else {
                    updateStatus('❌ Login failed: ' + data.error);
                }
            } catch (error) {
                updateStatus('❌ Network error: ' + error.message);
            }
        }

        function connectWebSocket() {
            ws = new WebSocket('ws://localhost:8080');

            ws.onopen = () => {
                updateStatus('🔌 Connected to game server');
                
                // JWT 토큰 전송
                ws.send(JSON.stringify({
                    type: 'auth',
                    token: accessToken
                }));
            };

            ws.onmessage = (event) => {
                const msg = JSON.parse(event.data);
                handleMessage(msg);
            };

            ws.onerror = () => {
                updateStatus('❌ WebSocket error');
            };

            ws.onclose = () => {
                updateStatus('🔌 Disconnected');
            };
        }

        function handleMessage(msg) {
            switch (msg.type) {
                case 'auth_required':
                    // 이미 onopen에서 전송했음
                    break;
                    
                case 'auth_success':
                    updateStatus('✅ Authenticated!');
                    break;
                    
                case 'game_joined':
                    updateStatus('🎮 Joined game!');
                    break;
                    
                case 'error':
                    updateStatus('❌ ' + msg.message);
                    
                    // 토큰 만료 시 갱신 시도
                    if (msg.message.includes('expired')) {
                        refreshAccessToken();
                    }
                    break;
            }
        }

        function joinGame() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    type: 'join_game'
                }));
            }
        }

        async function refreshAccessToken() {
            updateStatus('🔄 Refreshing token...');

            try {
                const response = await fetch('http://localhost:8081/refresh', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({
                        refresh_token: refreshToken
                    })
                });

                const data = await response.json();

                if (response.ok) {
                    accessToken = data.access_token;
                    localStorage.setItem('access_token', accessToken);
                    
                    updateStatus('✅ Token refreshed');
                    
                    // WebSocket 재연결
                    if (ws) ws.close();
                    connectWebSocket();
                } else {
                    updateStatus('❌ Token refresh failed, please login again');
                    logout();
                }
            } catch (error) {
                updateStatus('❌ Refresh error: ' + error.message);
            }
        }

        function logout() {
            accessToken = null;
            refreshToken = null;
            localStorage.removeItem('access_token');
            localStorage.removeItem('refresh_token');
            
            if (ws) {
                ws.close();
                ws = null;
            }
            
            loginForm.style.display = 'block';
            gameArea.style.display = 'none';
            
            updateStatus('Logged out');
        }

        // 페이지 로드 시 저장된 토큰 확인
        window.onload = () => {
            const savedToken = localStorage.getItem('access_token');
            if (savedToken) {
                accessToken = savedToken;
                refreshToken = localStorage.getItem('refresh_token');
                // 토큰 유효성 검증 후 자동 로그인
                // (간단화를 위해 생략)
            }
        };
    </script>
</body>
</html>
```

---

## Part 5: 보안 모범 사례 (10분)

### 5.1 환경 변수로 비밀 키 관리

```cpp
// config.h
#pragma once
#include <string>
#include <cstdlib>
#include <stdexcept>

class Config {
public:
    static std::string get_jwt_secret() {
        const char* secret = std::getenv("JWT_SECRET");
        if (!secret) {
            throw std::runtime_error("JWT_SECRET environment variable not set");
        }
        return std::string(secret);
    }
    
    static std::string get_database_url() {
        const char* url = std::getenv("DATABASE_URL");
        if (!url) {
            return "postgresql://localhost/gamedb";  // 기본값
        }
        return std::string(url);
    }
    
    static int get_token_expiry_hours() {
        const char* expiry = std::getenv("TOKEN_EXPIRY_HOURS");
        if (!expiry) {
            return 1;  // 기본 1시간
        }
        return std::stoi(expiry);
    }
};
```

**사용**:
```bash
export JWT_SECRET="your-super-secret-256-bit-key-change-this-in-production"
export TOKEN_EXPIRY_HOURS=2
./game_server
```

---

### 5.2 HTTPS/WSS 사용 (프로덕션)

```cpp
// secure_websocket_server.cpp
// TLS/SSL을 사용한 WSS (WebSocket Secure)
#include <boost/beast/ssl.hpp>

namespace ssl = boost::asio::ssl;

class SecureWebSocketSession {
    websocket::stream<ssl::stream<tcp::socket>> ws_;
    
public:
    SecureWebSocketSession(tcp::socket socket, ssl::context& ctx)
        : ws_(std::move(socket), ctx)
    {
    }
    
    void run() {
        // SSL handshake
        ws_.next_layer().async_handshake(
            ssl::stream_base::server,
            [self = shared_from_this()](beast::error_code ec) {
                if (!ec) {
                    // WebSocket handshake
                    self->ws_.async_accept(/* ... */);
                }
            });
    }
};

int main() {
    // SSL 컨텍스트 설정
    ssl::context ctx{ssl::context::tlsv12_server};
    ctx.use_certificate_chain_file("server.crt");
    ctx.use_private_key_file("server.key", ssl::context::pem);
    
    // ... 서버 실행
}
```

---

### 5.3 Rate Limiting (DDoS 방지)

```cpp
// rate_limiter.h
#pragma once
#include <unordered_map>
#include <chrono>
#include <mutex>

class RateLimiter {
private:
    struct Bucket {
        int tokens;
        std::chrono::steady_clock::time_point last_refill;
    };
    
    std::unordered_map<std::string, Bucket> buckets_;
    std::mutex mutex_;
    int max_tokens_;
    int refill_rate_;  // tokens per second

public:
    RateLimiter(int max_tokens = 10, int refill_rate = 1)
        : max_tokens_(max_tokens), refill_rate_(refill_rate)
    {
    }
    
    bool allow(const std::string& user_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto& bucket = buckets_[user_id];
        
        // 토큰 리필
        if (bucket.tokens < max_tokens_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - bucket.last_refill).count();
            bucket.tokens = std::min(
                max_tokens_,
                bucket.tokens + static_cast<int>(elapsed * refill_rate_)
            );
            bucket.last_refill = now;
        }
        
        // 토큰 소비
        if (bucket.tokens > 0) {
            bucket.tokens--;
            return true;
        }
        
        return false;
    }
};
```

**사용**:
```cpp
RateLimiter rate_limiter(10, 1);  // 10 tokens, 1/sec refill

if (!rate_limiter.allow(user_id)) {
    send_error("Rate limit exceeded");
    return;
}
```

---

## Troubleshooting

### 문제 1: "jwt-cpp/jwt.h: No such file or directory"

**증상**:
```
fatal error: jwt-cpp/jwt.h: No such file or directory
```

**원인**: jwt-cpp 라이브러리 미설치

**해결**:
```bash
# 방법 1: 패키지 설치 (Ubuntu)
sudo apt-get install libjwt-dev

# 방법 2: 헤더 다운로드 (헤더 전용)
mkdir -p include/jwt-cpp
cd include/jwt-cpp
wget https://raw.githubusercontent.com/Thalhammer/jwt-cpp/master/include/jwt-cpp/jwt.h
wget https://raw.githubusercontent.com/Thalhammer/jwt-cpp/master/include/jwt-cpp/base.h
wget https://raw.githubusercontent.com/Thalhammer/jwt-cpp/master/include/jwt-cpp/traits/kazuho-picojson/defaults.h

# CMakeLists.txt에 포함 경로 추가
target_include_directories(game_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

---

### 문제 2: "Token expired" 에러가 즉시 발생

**증상**:
토큰을 생성하자마자 만료됨

**원인**:
서버와 클라이언트 시간 동기화 문제

**해결**:
```cpp
// 토큰 생성 시 약간의 여유 시간 추가
auto token = jwt::create()
    .set_issued_at(std::chrono::system_clock::now() - std::chrono::seconds{10})  // 10초 전
    .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{1})
    // ...
    .sign(jwt::algorithm::hs256{secret});

// 검증 시 clock skew 허용
auto verifier = jwt::verify()
    .allow_algorithm(jwt::algorithm::hs256{secret})
    .with_issuer("game-server")
    .leeway(30);  // 30초 허용
```

---

### 문제 3: CORS 에러 (브라우저)

**증상**:
```
Access to fetch at 'http://localhost:8081/login' from origin 'null' 
has been blocked by CORS policy
```

**원인**:
CORS 헤더 누락

**해결**:
```cpp
// HTTP 응답에 CORS 헤더 추가
response.set(http::field::access_control_allow_origin, "*");
response.set(http::field::access_control_allow_methods, "POST, OPTIONS");
response.set(http::field::access_control_allow_headers, "Content-Type, Authorization");

// OPTIONS 요청 처리
if (request.method() == http::verb::options) {
    http::response<http::empty_body> res{http::status::ok, request.version()};
    // CORS 헤더 추가
    res.prepare_payload();
    return send_response(std::move(res));
}
```

---

### 문제 4: 토큰이 WebSocket 연결에서 검증 안됨

**증상**:
HTTP 로그인은 성공하지만 WebSocket 인증 실패

**원인**:
1. 토큰이 전송되지 않음
2. 토큰 형식 오류

**해결**:
```javascript
// 클라이언트에서 토큰 전송 확인
ws.onopen = () => {
    console.log('Sending token:', accessToken.substring(0, 20) + '...');
    ws.send(JSON.stringify({
        type: 'auth',
        token: accessToken  // Bearer 제거
    }));
};

// 서버에서 로깅 추가
void handle_auth(const nlohmann::json& msg) {
    std::string token = msg["token"];
    std::cout << "Received token: " << token.substring(0, 20) << "...\n";
    
    auto claims = jwt_validator_->validate(token);
    if (!claims) {
        std::cerr << "Token validation failed\n";
        // ...
    }
}
```

---

### 문제 5: Refresh 토큰이 작동하지 않음

**증상**:
Refresh 토큰으로 새 access token을 받을 수 없음

**원인**:
Refresh 토큰 타입 클레임 누락

**해결**:
```cpp
// Refresh 토큰 생성 시 타입 명시
auto refresh_token = jwt::create()
    .set_payload_claim("type", jwt::claim(std::string("refresh")))  // 중요!
    .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{24 * 7})
    // ...
    .sign(jwt::algorithm::hs256{secret});

// 검증 시 타입 확인
bool validate_refresh_token(const std::string& token) {
    auto decoded = jwt::decode(token);
    
    // 타입 클레임 확인
    if (!decoded.has_payload_claim("type")) {
        return false;
    }
    
    auto type = decoded.get_payload_claim("type").as_string();
    if (type != "refresh") {
        return false;
    }
    
    // ...
}
```

---

## 요약

이번 Quickstart에서 학습한 내용:

1. **jwt-cpp 라이브러리**: JWT 생성 및 검증
2. **JWTValidator 클래스**: 재사용 가능한 JWT 검증 로직
3. **WebSocket 인증**: 연결 시 JWT 토큰 검증
4. **인증 서버**: HTTP 로그인 API, Refresh Token
5. **보안 모범 사례**: 환경 변수, HTTPS, Rate Limiting

**mini-gameserver Milestone 1.7 완료!** ✅

**다음 단계**:
- 44-elo-db-integration.md: ELO 랭킹 + PostgreSQL
- 45-matchmaking-system.md: 매치메이킹 큐

**주요 개념**:
- JWT는 stateless 인증 제공 (서버가 세션 저장 불필요)
- Access Token (짧음) + Refresh Token (김) 패턴
- WebSocket 연결 시 초기 인증 필수
- 환경 변수로 비밀 키 관리
- Rate Limiting으로 남용 방지

이제 JWT 기반 인증이 통합된 보안 게임 서버를 만들 수 있습니다! 🔐🎮
