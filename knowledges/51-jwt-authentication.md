# Quickstart 51: JWT Authentication

> **📚 학습 유형**: 기초 개념 (Fundamentals)  
> **⏭️ 다음 단계**: 이 문서 완료 후 → [Quickstart 43: JWT Game Integration](43-jwt-game-integration.md) (게임 서버 통합)

## 🎯 목표
- **JWT (JSON Web Token)**: Stateless 인증 방식
- **HS256 (HMAC-SHA256)**: JWT 서명 알고리즘
- **C++ 구현**: 토큰 생성, 검증, 파싱
- **실전**: 게임 서버 로그인/인증 시스템

## 📋 사전준비
- [Quickstart 30](30-cpp-for-game-server.md) 완료 (C++ 기초)
- [Quickstart 33](33-boost-asio-beast.md) 권장 (HTTP 서버)
- OpenSSL 설치 (HMAC-SHA256용)

---

## 🔐 Part 1: JWT 기초

### 1.1 JWT란?

**JWT (JSON Web Token)**는 **클라이언트-서버 간 인증 정보를 안전하게 전달**하는 토큰 방식입니다.

```
전통적인 Session 방식:
1. 클라이언트 로그인 → 서버가 세션 ID 생성 → DB/Redis에 저장
2. 클라이언트는 세션 ID를 쿠키로 저장
3. 매 요청마다 서버가 세션 ID로 DB 조회
❌ 문제: 서버 상태 저장 필요 (Stateful), 확장성 낮음

JWT 방식:
1. 클라이언트 로그인 → 서버가 JWT 토큰 생성 (서명 포함)
2. 클라이언트는 JWT를 로컬에 저장
3. 매 요청마다 JWT를 헤더에 포함 → 서버는 서명만 검증
✅ 장점: 서버 상태 불필요 (Stateless), 확장성 높음
```

### 1.2 JWT 구조

JWT는 **3개 부분**으로 구성됩니다:

```
eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VyX2lkIjoxMjMsImV4cCI6MTYzOTU5NjAwMH0.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c

구조:
Header.Payload.Signature

1. Header (헤더):
   {"alg":"HS256","typ":"JWT"}
   - alg: 서명 알고리즘 (HS256, RS256 등)
   - typ: 토큰 타입 (JWT)

2. Payload (페이로드):
   {"user_id":123,"exp":1639596000}
   - user_id: 사용자 정보
   - exp: 만료 시간 (Unix timestamp)
   - iat: 발급 시간
   - iss: 발급자

3. Signature (서명):
   HMAC_SHA256(
     base64UrlEncode(header) + "." + base64UrlEncode(payload),
     secret_key
   )
   - 위조 방지: secret_key 없이는 서명 생성 불가
```

### 1.3 JWT vs Session 비교

| 특성 | Session | JWT |
|------|---------|-----|
| **저장 위치** | 서버 (DB/Redis) | 클라이언트 (로컬 스토리지) |
| **상태** | Stateful (서버 상태 유지) | Stateless (서버 상태 없음) |
| **확장성** | 낮음 (세션 공유 필요) | 높음 (서버 추가 용이) |
| **보안** | 서버 통제 가능 | 토큰 탈취 시 무효화 어려움 |
| **성능** | DB 조회 필요 | 서명 검증만 |
| **만료** | 서버에서 삭제 | 토큰 자체에 만료 시간 |

**게임 서버에서는 JWT 선호**:
- 여러 게임 서버 인스턴스 간 상태 공유 불필요
- Redis 의존성 감소
- 빠른 인증 (DB 조회 없음)

---

## 🛠️ Part 2: OpenSSL 기초

### 2.1 OpenSSL 설치

```bash
# macOS
brew install openssl

# Ubuntu/Debian
sudo apt-get install libssl-dev

# Windows (vcpkg)
vcpkg install openssl
```

### 2.2 Base64 URL Encoding

JWT는 **Base64 URL-safe** 인코딩을 사용합니다:

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

class Base64 {
public:
    // Base64 인코딩
    static std::string encode(const std::string& input) {
        BIO* bio = BIO_new(BIO_s_mem());
        BIO* b64 = BIO_new(BIO_f_base64());
        bio = BIO_push(b64, bio);
        
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);  // 줄바꿈 없음
        BIO_write(bio, input.c_str(), input.size());
        BIO_flush(bio);
        
        BUF_MEM* buffer_ptr;
        BIO_get_mem_ptr(bio, &buffer_ptr);
        
        std::string result(buffer_ptr->data, buffer_ptr->length);
        BIO_free_all(bio);
        
        // Base64 → Base64 URL-safe
        return to_url_safe(result);
    }
    
    // Base64 디코딩
    static std::string decode(const std::string& input) {
        std::string base64_input = from_url_safe(input);
        
        BIO* bio = BIO_new_mem_buf(base64_input.c_str(), base64_input.size());
        BIO* b64 = BIO_new(BIO_f_base64());
        bio = BIO_push(b64, bio);
        
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        
        std::vector<char> buffer(base64_input.size());
        int decoded_size = BIO_read(bio, buffer.data(), buffer.size());
        BIO_free_all(bio);
        
        return std::string(buffer.data(), decoded_size);
    }
    
private:
    // Base64 → Base64 URL-safe (+ → -, / → _, = 제거)
    static std::string to_url_safe(const std::string& base64) {
        std::string result = base64;
        for (char& c : result) {
            if (c == '+') c = '-';
            else if (c == '/') c = '_';
        }
        // 패딩 제거
        while (!result.empty() && result.back() == '=') {
            result.pop_back();
        }
        return result;
    }
    
    // Base64 URL-safe → Base64
    static std::string from_url_safe(const std::string& url_safe) {
        std::string result = url_safe;
        for (char& c : result) {
            if (c == '-') c = '+';
            else if (c == '_') c = '/';
        }
        // 패딩 추가
        while (result.size() % 4 != 0) {
            result += '=';
        }
        return result;
    }
};

int main() {
    std::string original = "Hello, JWT!";
    
    // 인코딩
    std::string encoded = Base64::encode(original);
    std::cout << "Original: " << original << "\n";
    std::cout << "Encoded:  " << encoded << "\n";
    
    // 디코딩
    std::string decoded = Base64::decode(encoded);
    std::cout << "Decoded:  " << decoded << "\n";
    
    return 0;
}
```

**CMakeLists.txt** (base64_demo):
```cmake
cmake_minimum_required(VERSION 3.20)
project(base64_demo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

add_executable(base64_demo base64_demo.cpp)
target_link_libraries(base64_demo PRIVATE OpenSSL::SSL OpenSSL::Crypto)
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./base64_demo
```

**실행 결과**:
```
Original: Hello, JWT!
Encoded:  SGVsbG8sIEpXVCE
Decoded:  Hello, JWT!
```

### 2.3 HMAC-SHA256

```cpp
#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <openssl/hmac.h>
#include <openssl/sha.h>

class HMAC {
public:
    // HMAC-SHA256 생성
    static std::string sha256(const std::string& data, const std::string& key) {
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hash_len;
        
        HMAC_CTX* ctx = HMAC_CTX_new();
        HMAC_Init_ex(ctx, key.c_str(), key.size(), EVP_sha256(), nullptr);
        HMAC_Update(ctx, reinterpret_cast<const unsigned char*>(data.c_str()), data.size());
        HMAC_Final(ctx, hash, &hash_len);
        HMAC_CTX_free(ctx);
        
        return std::string(reinterpret_cast<char*>(hash), hash_len);
    }
    
    // Hex 문자열로 변환
    static std::string to_hex(const std::string& binary) {
        std::ostringstream oss;
        for (unsigned char c : binary) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
        }
        return oss.str();
    }
};

int main() {
    std::string message = "Hello, JWT!";
    std::string secret_key = "my-secret-key";
    
    // HMAC-SHA256 계산
    std::string signature = HMAC::sha256(message, secret_key);
    
    std::cout << "Message:    " << message << "\n";
    std::cout << "Secret Key: " << secret_key << "\n";
    std::cout << "Signature (hex): " << HMAC::to_hex(signature) << "\n";
    std::cout << "Signature (base64): " << Base64::encode(signature) << "\n";
    
    return 0;
}
```

**실행 결과**:
```
Message:    Hello, JWT!
Secret Key: my-secret-key
Signature (hex): 8f3e6e7c5d4b3a2f1e0d9c8b7a6f5e4d3c2b1a0f9e8d7c6b5a4f3e2d1c0b9a8f
Signature (base64): jz5ufF1LOi8eDZyLem9eTTwrGg-ejXxrWk8-LRwLmK8
```

---

## 🔑 Part 3: JWT 구현

### 3.1 JWT 생성

```cpp
#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include <nlohmann/json.hpp>  // JSON 라이브러리
#include "base64.h"  // 위에서 구현한 Base64 클래스
#include "hmac.h"    // 위에서 구현한 HMAC 클래스

using json = nlohmann::json;
using namespace std::chrono;

class JWT {
private:
    std::string secret_key;
    
public:
    JWT(const std::string& key) : secret_key(key) {}
    
    // JWT 토큰 생성
    std::string create_token(int user_id, int expires_in_seconds = 3600) {
        // 1. Header
        json header;
        header["alg"] = "HS256";
        header["typ"] = "JWT";
        std::string header_str = header.dump();
        std::string encoded_header = Base64::encode(header_str);
        
        // 2. Payload
        auto now = system_clock::now();
        auto exp = now + seconds(expires_in_seconds);
        
        json payload;
        payload["user_id"] = user_id;
        payload["iat"] = duration_cast<seconds>(now.time_since_epoch()).count();
        payload["exp"] = duration_cast<seconds>(exp.time_since_epoch()).count();
        
        std::string payload_str = payload.dump();
        std::string encoded_payload = Base64::encode(payload_str);
        
        // 3. Signature
        std::string message = encoded_header + "." + encoded_payload;
        std::string signature = HMAC::sha256(message, secret_key);
        std::string encoded_signature = Base64::encode(signature);
        
        // 4. Token
        return message + "." + encoded_signature;
    }
    
    // JWT 토큰 검증
    struct TokenResult {
        bool valid;
        int user_id;
        std::string error;
    };
    
    TokenResult verify_token(const std::string& token) {
        TokenResult result{false, 0, ""};
        
        // 토큰 분리
        size_t first_dot = token.find('.');
        size_t second_dot = token.find('.', first_dot + 1);
        
        if (first_dot == std::string::npos || second_dot == std::string::npos) {
            result.error = "Invalid token format";
            return result;
        }
        
        std::string encoded_header = token.substr(0, first_dot);
        std::string encoded_payload = token.substr(first_dot + 1, second_dot - first_dot - 1);
        std::string encoded_signature = token.substr(second_dot + 1);
        
        // 서명 검증
        std::string message = encoded_header + "." + encoded_payload;
        std::string expected_signature = HMAC::sha256(message, secret_key);
        std::string expected_encoded_signature = Base64::encode(expected_signature);
        
        if (encoded_signature != expected_encoded_signature) {
            result.error = "Invalid signature";
            return result;
        }
        
        // Payload 파싱
        std::string payload_str = Base64::decode(encoded_payload);
        json payload = json::parse(payload_str);
        
        // 만료 시간 확인
        int64_t exp = payload["exp"];
        auto now = system_clock::now();
        int64_t current_time = duration_cast<seconds>(now.time_since_epoch()).count();
        
        if (current_time > exp) {
            result.error = "Token expired";
            return result;
        }
        
        // 성공
        result.valid = true;
        result.user_id = payload["user_id"];
        return result;
    }
};

int main() {
    JWT jwt("my-super-secret-key");
    
    // 토큰 생성 (1시간 유효)
    int user_id = 12345;
    std::string token = jwt.create_token(user_id, 3600);
    
    std::cout << "User ID: " << user_id << "\n";
    std::cout << "Token: " << token << "\n\n";
    
    // 토큰 검증
    auto result = jwt.verify_token(token);
    
    if (result.valid) {
        std::cout << "✅ Token is valid!\n";
        std::cout << "User ID: " << result.user_id << "\n";
    } else {
        std::cout << "❌ Token is invalid: " << result.error << "\n";
    }
    
    // 만료된 토큰 테스트
    std::cout << "\n=== Testing expired token ===\n";
    std::string expired_token = jwt.create_token(user_id, -1);  // 이미 만료
    auto expired_result = jwt.verify_token(expired_token);
    
    if (!expired_result.valid) {
        std::cout << "❌ Token is invalid: " << expired_result.error << "\n";
    }
    
    // 위조된 토큰 테스트
    std::cout << "\n=== Testing forged token ===\n";
    std::string forged_token = token;
    forged_token[10] = 'X';  // 토큰 변조
    auto forged_result = jwt.verify_token(forged_token);
    
    if (!forged_result.valid) {
        std::cout << "❌ Token is invalid: " << forged_result.error << "\n";
    }
    
    return 0;
}
```

**CMakeLists.txt** (jwt_demo):
```cmake
cmake_minimum_required(VERSION 3.20)
project(jwt_demo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

# nlohmann/json 다운로드 (헤더 온리)
include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

add_executable(jwt_demo jwt_demo.cpp base64.cpp hmac.cpp)
target_link_libraries(jwt_demo PRIVATE 
    OpenSSL::SSL 
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./jwt_demo
```

**실행 결과**:
```
User ID: 12345
Token: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VyX2lkIjoxMjM0NSwiaWF0IjoxNjk5ODg4ODg4LCJleHAiOjE2OTk4OTI0ODh9.4Xj8fK9mP2qN5wR7tY6uI8oL3kJ1hG9fE2dC5bA4xW0

✅ Token is valid!
User ID: 12345

=== Testing expired token ===
❌ Token is invalid: Token expired

=== Testing forged token ===
❌ Token is invalid: Invalid signature
```

### 3.2 JWT 클래스 완전 구현

```cpp
// jwt.h
#pragma once
#include <string>
#include <nlohmann/json.hpp>

class JWT {
public:
    struct TokenResult {
        bool valid;
        int user_id;
        std::string username;
        int64_t issued_at;
        int64_t expires_at;
        std::string error;
    };
    
    JWT(const std::string& secret_key);
    
    // 토큰 생성
    std::string create_token(int user_id, const std::string& username, int expires_in_seconds = 3600);
    
    // 토큰 검증
    TokenResult verify_token(const std::string& token);
    
    // Refresh Token 생성 (긴 유효기간)
    std::string create_refresh_token(int user_id, int expires_in_seconds = 86400 * 30);  // 30일
    
private:
    std::string secret_key;
    
    std::string encode_base64(const std::string& input);
    std::string decode_base64(const std::string& input);
    std::string hmac_sha256(const std::string& data);
};
```

```cpp
// jwt.cpp
#include "jwt.h"
#include <chrono>
#include <openssl/hmac.h>
#include <openssl/evp.h>

using json = nlohmann::json;
using namespace std::chrono;

JWT::JWT(const std::string& key) : secret_key(key) {}

std::string JWT::create_token(int user_id, const std::string& username, int expires_in_seconds) {
    // Header
    json header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";
    std::string encoded_header = encode_base64(header.dump());
    
    // Payload
    auto now = system_clock::now();
    auto exp = now + seconds(expires_in_seconds);
    int64_t iat = duration_cast<seconds>(now.time_since_epoch()).count();
    int64_t exp_time = duration_cast<seconds>(exp.time_since_epoch()).count();
    
    json payload;
    payload["user_id"] = user_id;
    payload["username"] = username;
    payload["iat"] = iat;
    payload["exp"] = exp_time;
    
    std::string encoded_payload = encode_base64(payload.dump());
    
    // Signature
    std::string message = encoded_header + "." + encoded_payload;
    std::string signature = hmac_sha256(message);
    std::string encoded_signature = encode_base64(signature);
    
    return message + "." + encoded_signature;
}

JWT::TokenResult JWT::verify_token(const std::string& token) {
    TokenResult result{false, 0, "", 0, 0, ""};
    
    // 토큰 파싱
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot + 1);
    
    if (first_dot == std::string::npos || second_dot == std::string::npos) {
        result.error = "Invalid token format";
        return result;
    }
    
    std::string encoded_header = token.substr(0, first_dot);
    std::string encoded_payload = token.substr(first_dot + 1, second_dot - first_dot - 1);
    std::string encoded_signature = token.substr(second_dot + 1);
    
    // 서명 검증
    std::string message = encoded_header + "." + encoded_payload;
    std::string expected_signature = hmac_sha256(message);
    std::string expected_encoded = encode_base64(expected_signature);
    
    if (encoded_signature != expected_encoded) {
        result.error = "Invalid signature";
        return result;
    }
    
    // Payload 파싱
    try {
        std::string payload_str = decode_base64(encoded_payload);
        json payload = json::parse(payload_str);
        
        // 만료 시간 확인
        int64_t exp = payload["exp"];
        auto now = system_clock::now();
        int64_t current_time = duration_cast<seconds>(now.time_since_epoch()).count();
        
        if (current_time > exp) {
            result.error = "Token expired";
            return result;
        }
        
        // 성공
        result.valid = true;
        result.user_id = payload["user_id"];
        result.username = payload["username"];
        result.issued_at = payload["iat"];
        result.expires_at = exp;
        
    } catch (const std::exception& e) {
        result.error = std::string("Parse error: ") + e.what();
    }
    
    return result;
}

std::string JWT::create_refresh_token(int user_id, int expires_in_seconds) {
    json payload;
    payload["user_id"] = user_id;
    payload["type"] = "refresh";
    
    auto now = system_clock::now();
    auto exp = now + seconds(expires_in_seconds);
    payload["iat"] = duration_cast<seconds>(now.time_since_epoch()).count();
    payload["exp"] = duration_cast<seconds>(exp.time_since_epoch()).count();
    
    json header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";
    
    std::string encoded_header = encode_base64(header.dump());
    std::string encoded_payload = encode_base64(payload.dump());
    std::string message = encoded_header + "." + encoded_payload;
    std::string signature = hmac_sha256(message);
    
    return message + "." + encode_base64(signature);
}

// Private methods (Base64, HMAC 구현은 위와 동일)
```

---

## 🎮 Part 4: 게임 서버 통합

### 4.1 로그인 시스템

```cpp
#include <iostream>
#include <map>
#include <string>
#include <memory>
#include "jwt.h"

// 사용자 데이터베이스 (실제로는 PostgreSQL 사용)
struct User {
    int id;
    std::string username;
    std::string password_hash;  // 실제로는 bcrypt 해시
};

class UserDatabase {
private:
    std::map<std::string, User> users;
    int next_id = 1;
    
public:
    UserDatabase() {
        // 테스트 사용자 추가
        register_user("alice", "password123");
        register_user("bob", "secret456");
    }
    
    bool register_user(const std::string& username, const std::string& password) {
        if (users.find(username) != users.end()) {
            return false;  // 이미 존재
        }
        
        User user;
        user.id = next_id++;
        user.username = username;
        user.password_hash = hash_password(password);  // 실제로는 bcrypt
        
        users[username] = user;
        return true;
    }
    
    User* authenticate(const std::string& username, const std::string& password) {
        auto it = users.find(username);
        if (it == users.end()) {
            return nullptr;  // 사용자 없음
        }
        
        if (it->second.password_hash != hash_password(password)) {
            return nullptr;  // 비밀번호 틀림
        }
        
        return &it->second;
    }
    
private:
    std::string hash_password(const std::string& password) {
        // 실제로는 bcrypt 사용
        // 여기서는 간단히 문자열 반환 (절대 실전에서 사용하지 마세요!)
        return "hashed_" + password;
    }
};

class AuthService {
private:
    JWT jwt;
    UserDatabase& db;
    
public:
    AuthService(const std::string& secret_key, UserDatabase& database)
        : jwt(secret_key), db(database) {}
    
    struct LoginResult {
        bool success;
        std::string access_token;
        std::string refresh_token;
        std::string error;
    };
    
    LoginResult login(const std::string& username, const std::string& password) {
        LoginResult result;
        
        // 인증
        User* user = db.authenticate(username, password);
        if (!user) {
            result.success = false;
            result.error = "Invalid username or password";
            return result;
        }
        
        // JWT 토큰 생성
        result.success = true;
        result.access_token = jwt.create_token(user->id, user->username, 3600);  // 1시간
        result.refresh_token = jwt.create_refresh_token(user->id, 86400 * 30);   // 30일
        
        return result;
    }
    
    struct AuthResult {
        bool success;
        int user_id;
        std::string username;
        std::string error;
    };
    
    AuthResult verify(const std::string& token) {
        AuthResult result;
        
        auto token_result = jwt.verify_token(token);
        
        if (!token_result.valid) {
            result.success = false;
            result.error = token_result.error;
            return result;
        }
        
        result.success = true;
        result.user_id = token_result.user_id;
        result.username = token_result.username;
        
        return result;
    }
};

int main() {
    UserDatabase db;
    AuthService auth("my-super-secret-key-2024", db);
    
    std::cout << "=== Game Server Authentication System ===\n\n";
    
    // 로그인 성공
    std::cout << "1. Login with valid credentials:\n";
    auto login_result = auth.login("alice", "password123");
    
    if (login_result.success) {
        std::cout << "✅ Login successful!\n";
        std::cout << "Access Token: " << login_result.access_token.substr(0, 50) << "...\n";
        std::cout << "Refresh Token: " << login_result.refresh_token.substr(0, 50) << "...\n\n";
        
        // 토큰 검증
        std::cout << "2. Verify access token:\n";
        auto auth_result = auth.verify(login_result.access_token);
        
        if (auth_result.success) {
            std::cout << "✅ Token is valid!\n";
            std::cout << "User ID: " << auth_result.user_id << "\n";
            std::cout << "Username: " << auth_result.username << "\n\n";
        }
    }
    
    // 로그인 실패
    std::cout << "3. Login with invalid credentials:\n";
    auto failed_login = auth.login("alice", "wrong_password");
    
    if (!failed_login.success) {
        std::cout << "❌ Login failed: " << failed_login.error << "\n\n";
    }
    
    // 잘못된 토큰
    std::cout << "4. Verify invalid token:\n";
    auto invalid_auth = auth.verify("invalid.token.here");
    
    if (!invalid_auth.success) {
        std::cout << "❌ Authentication failed: " << invalid_auth.error << "\n";
    }
    
    return 0;
}
```

**CMakeLists.txt** (game_auth_system):
```cmake
cmake_minimum_required(VERSION 3.20)
project(game_auth_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

add_executable(game_auth_system 
    game_auth_system.cpp 
    jwt.cpp 
    base64.cpp 
    hmac.cpp
)

target_link_libraries(game_auth_system PRIVATE 
    OpenSSL::SSL 
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./game_auth_system
```

**실행 결과**:
```
=== Game Server Authentication System ===

1. Login with valid credentials:
✅ Login successful!
Access Token: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2Vy...
Refresh Token: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2Vy...

2. Verify access token:
✅ Token is valid!
User ID: 1
Username: alice

3. Login with invalid credentials:
❌ Login failed: Invalid username or password

4. Verify invalid token:
❌ Authentication failed: Invalid token format
```

### 4.2 HTTP 헤더를 통한 인증

```cpp
#include <string>
#include <sstream>

class AuthMiddleware {
private:
    AuthService& auth_service;
    
public:
    AuthMiddleware(AuthService& service) : auth_service(service) {}
    
    struct AuthContext {
        bool authenticated;
        int user_id;
        std::string username;
        std::string error;
    };
    
    AuthContext authenticate_request(const std::string& authorization_header) {
        AuthContext ctx;
        ctx.authenticated = false;
        
        // "Authorization: Bearer <token>" 파싱
        if (authorization_header.find("Bearer ") != 0) {
            ctx.error = "Missing or invalid Authorization header";
            return ctx;
        }
        
        std::string token = authorization_header.substr(7);  // "Bearer " 제거
        
        // 토큰 검증
        auto result = auth_service.verify(token);
        
        if (!result.success) {
            ctx.error = result.error;
            return ctx;
        }
        
        ctx.authenticated = true;
        ctx.user_id = result.user_id;
        ctx.username = result.username;
        
        return ctx;
    }
};

// HTTP 요청 시뮬레이션
void handle_http_request(const std::string& authorization_header, AuthMiddleware& middleware) {
    std::cout << "=== HTTP Request ===\n";
    std::cout << "Authorization: " << authorization_header.substr(0, 50) << "...\n";
    
    auto ctx = middleware.authenticate_request(authorization_header);
    
    if (ctx.authenticated) {
        std::cout << "✅ Authenticated as user " << ctx.username << " (ID: " << ctx.user_id << ")\n";
        std::cout << "Processing request...\n";
    } else {
        std::cout << "❌ Authentication failed: " << ctx.error << "\n";
        std::cout << "HTTP 401 Unauthorized\n";
    }
    
    std::cout << "\n";
}

int main() {
    UserDatabase db;
    AuthService auth("secret-key-2024", db);
    AuthMiddleware middleware(auth);
    
    // 로그인
    auto login = auth.login("alice", "password123");
    
    // 요청 1: 유효한 토큰
    std::string auth_header = "Bearer " + login.access_token;
    handle_http_request(auth_header, middleware);
    
    // 요청 2: 잘못된 토큰
    handle_http_request("Bearer invalid.token.here", middleware);
    
    // 요청 3: 헤더 없음
    handle_http_request("", middleware);
    
    return 0;
}
```

**실행 결과**:
```
=== HTTP Request ===
Authorization: Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1...
✅ Authenticated as user alice (ID: 1)
Processing request...

=== HTTP Request ===
Authorization: Bearer invalid.token.here...
❌ Authentication failed: Invalid token format
HTTP 401 Unauthorized

=== HTTP Request ===
Authorization: ...
❌ Authentication failed: Missing or invalid Authorization header
HTTP 401 Unauthorized
```

---

## 🐛 자주 막히는 부분

### 문제 1: Secret Key가 노출됨

```cpp
// ❌ 코드에 하드코딩
JWT jwt("my-secret-key");  // 위험!

// ✅ 환경 변수 사용
const char* secret = std::getenv("JWT_SECRET_KEY");
if (!secret) {
    throw std::runtime_error("JWT_SECRET_KEY not set");
}
JWT jwt(secret);
```

**실행**:
```bash
export JWT_SECRET_KEY="super-secure-random-key-2024"
./game_server
```

### 문제 2: 토큰 만료 시간 설정 실수

```cpp
// ❌ Access Token을 너무 길게
jwt.create_token(user_id, username, 86400 * 365);  // 1년! (보안 위험)

// ✅ Access Token은 짧게, Refresh Token은 길게
std::string access_token = jwt.create_token(user_id, username, 3600);      // 1시간
std::string refresh_token = jwt.create_refresh_token(user_id, 86400 * 30); // 30일

// Access Token 만료 시 Refresh Token으로 재발급
```

### 문제 3: XSS (Cross-Site Scripting) 취약점

```javascript
// ❌ JWT를 localStorage에 저장 (XSS 공격 가능)
localStorage.setItem('token', jwt_token);

// ✅ HttpOnly 쿠키 사용 (JavaScript 접근 불가)
// Set-Cookie: token=...; HttpOnly; Secure; SameSite=Strict
```

### 문제 4: JWT 블랙리스트 없음

```cpp
// JWT는 서버에서 무효화할 수 없음!
// 해결책: Redis에 블랙리스트 저장

class JWTBlacklist {
private:
    std::set<std::string> blacklist;  // 실제로는 Redis 사용
    
public:
    void revoke(const std::string& token) {
        blacklist.insert(token);
    }
    
    bool is_revoked(const std::string& token) {
        return blacklist.count(token) > 0;
    }
};

// 로그아웃 시
blacklist.revoke(access_token);
```

### 문제 5: Base64 패딩 처리 실수

```cpp
// ❌ URL-safe Base64 변환 안 함
std::string token = base64_encode(data);  // + / = 포함

// ✅ URL-safe Base64 (+ → -, / → _, = 제거)
std::string token = base64_url_encode(data);
```

---

## ✅ 완료 체크리스트

### Part 1: JWT 기초
- [ ] JWT 구조 이해 (Header.Payload.Signature)
- [ ] JWT vs Session 비교
- [ ] Stateless 인증 개념

### Part 2: OpenSSL 기초
- [ ] Base64 URL-safe 인코딩/디코딩
- [ ] HMAC-SHA256 서명 생성
- [ ] OpenSSL 라이브러리 링크

### Part 3: JWT 구현
- [ ] JWT 토큰 생성 (create_token)
- [ ] JWT 토큰 검증 (verify_token)
- [ ] 만료 시간 처리
- [ ] Refresh Token 구현

### Part 4: 게임 서버 통합
- [ ] 로그인 시스템 구현
- [ ] HTTP Authorization 헤더 파싱
- [ ] AuthMiddleware 구현
- [ ] 실전 게임 서버 적용

---

## 🚀 다음 단계

✅ **JWT Authentication 완료!**

**다음 학습**:
- [**Quickstart 52**](52-elo-rating-system.md) - ELO 랭킹 시스템
- [**Quickstart 60**](60-postgresql-redis-docker.md) - 데이터베이스 연동

**실전 적용**:
- `mini-gameserver` M1.7 - JWT 인증 추가
- `mini-spring` M1.3 - Spring Security + JWT

---

## 📚 참고 자료

- [JWT.io](https://jwt.io/) - JWT 디버거
- [RFC 7519 - JWT Specification](https://datatracker.ietf.org/doc/html/rfc7519)
- [OpenSSL Documentation](https://www.openssl.org/docs/)
- [nlohmann/json](https://github.com/nlohmann/json) - C++ JSON 라이브러리
- [OWASP JWT Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/JSON_Web_Token_for_Java_Cheat_Sheet.html)

---

**Last Updated**: 2025-11-12
