# Quickstart 60: PostgreSQL + Redis + Docker

> **📚 학습 유형**: 기초 개념 (Fundamentals)
> **⏭️ 다음 단계**: 이 문서 완료 후 → [Quickstart 44: ELO DB Integration](44-elo-db-integration.md)

## 🎯 목표
- **PostgreSQL**: 관계형 데이터베이스 설정 및 C++ 연동
- **Redis**: 인메모리 캐시/큐 설정 및 C++ 연동
- **Docker**: 개발 환경 컨테이너화
- **실전**: 게임 서버 데이터 저장 및 캐싱

## 📋 사전준비
- [Quickstart 30](30-cpp-for-game-server.md) 완료 (C++ 기초)
- Docker Desktop 설치 (macOS/Windows) 또는 Docker Engine (Linux)

---

## 🐘 Part 1: PostgreSQL 기초 (25분)

### 1.1 PostgreSQL이란?

**PostgreSQL**은 **오픈소스 관계형 데이터베이스**로, 게임 서버에서 사용자 정보, 매치 기록, 랭킹을 저장하는 데 사용됩니다.

```
왜 PostgreSQL?
- 트랜잭션 지원 (ACID)
- 복잡한 쿼리 처리 (JOIN, 집계)
- 확장성 (수백만 행 처리 가능)
- 무료 및 안정적
```

### 1.2 Docker로 PostgreSQL 실행

**docker-compose.yml**:
```yaml
version: '3.8'

services:
  postgres:
    image: postgres:15-alpine
    container_name: arena60-postgres
    environment:
      POSTGRES_DB: gamedb
      POSTGRES_USER: gameuser
      POSTGRES_PASSWORD: gamepass123
    ports:
      - "5432:5432"
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./migrations:/docker-entrypoint-initdb.d
    restart: unless-stopped

volumes:
  postgres_data:
```

**실행**:
```bash
# Docker Compose 시작
docker-compose up -d

# PostgreSQL 접속 확인
docker exec -it arena60-postgres psql -U gameuser -d gamedb

# 버전 확인
SELECT version();

# 종료
\q
```

### 1.3 스키마 생성

**migrations/001_init.sql**:
```sql
-- 사용자 테이블
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    elo_rating INTEGER DEFAULT 1000,
    games_played INTEGER DEFAULT 0,
    wins INTEGER DEFAULT 0,
    losses INTEGER DEFAULT 0,
    win_rate FLOAT DEFAULT 0.0,
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL DEFAULT NOW()
);

-- 인덱스
CREATE INDEX idx_users_username ON users(username);
CREATE INDEX idx_users_elo_rating ON users(elo_rating DESC);

-- 샘플 데이터
INSERT INTO users (username, email, password_hash, elo_rating) VALUES
    ('alice', 'alice@example.com', '$2a$10$hashed', 1200),
    ('bob', 'bob@example.com', '$2a$10$hashed', 1150),
    ('charlie', 'charlie@example.com', '$2a$10$hashed', 1300)
ON CONFLICT (username) DO NOTHING;
```

**실행**:
```bash
# 마이그레이션 실행
docker exec -i arena60-postgres psql -U gameuser -d gamedb < migrations/001_init.sql

# 확인
docker exec -it arena60-postgres psql -U gameuser -d gamedb -c "SELECT * FROM users;"
```

---

## 🔧 Part 2: libpqxx - C++ PostgreSQL 클라이언트 (30분)

### 2.1 libpqxx 설치

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install libpqxx-dev

# macOS
brew install libpqxx

# 확인
pkg-config --modversion libpqxx
```

### 2.2 간단한 연결 예제

**pg_example.cpp**:
```cpp
#include <iostream>
#include <pqxx/pqxx>

int main() {
    try {
        // 데이터베이스 연결
        pqxx::connection conn(
            "host=localhost "
            "port=5432 "
            "dbname=gamedb "
            "user=gameuser "
            "password=gamepass123"
        );

        if (!conn.is_open()) {
            std::cerr << "Failed to open database\n";
            return 1;
        }

        std::cout << "✅ Connected to PostgreSQL: " << conn.dbname() << "\n";

        // 트랜잭션 시작
        pqxx::work txn(conn);

        // 쿼리 실행
        pqxx::result result = txn.exec("SELECT * FROM users ORDER BY elo_rating DESC LIMIT 5");

        std::cout << "\n=== Top 5 Players ===\n";
        for (const auto& row : result) {
            std::cout << row["username"].as<std::string>() << ": "
                      << row["elo_rating"].as<int>() << "\n";
        }

        txn.commit();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.20)
project(pg_example)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PostgreSQL REQUIRED)

add_executable(pg_example pg_example.cpp)
target_link_libraries(pg_example PRIVATE pqxx PostgreSQL::PostgreSQL)
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./pg_example
```

**출력**:
```
✅ Connected to PostgreSQL: gamedb

=== Top 5 Players ===
charlie: 1300
alice: 1200
bob: 1150
```

### 2.3 파라미터화된 쿼리 (SQL Injection 방지)

```cpp
#include <pqxx/pqxx>
#include <iostream>
#include <optional>

struct User {
    int id;
    std::string username;
    std::string email;
    int elo_rating;
};

class UserRepository {
private:
    pqxx::connection conn_;

public:
    UserRepository(const std::string& conn_str) : conn_(conn_str) {}

    // 사용자 조회 (안전한 쿼리)
    std::optional<User> find_by_username(const std::string& username) {
        try {
            pqxx::work txn(conn_);

            // ❌ 위험: SQL Injection 가능
            // auto result = txn.exec("SELECT * FROM users WHERE username = '" + username + "'");

            // ✅ 안전: 파라미터화된 쿼리
            auto result = txn.exec_params(
                "SELECT id, username, email, elo_rating FROM users WHERE username = $1",
                username
            );

            if (result.empty()) {
                return std::nullopt;
            }

            auto row = result[0];
            User user;
            user.id = row["id"].as<int>();
            user.username = row["username"].as<std::string>();
            user.email = row["email"].as<std::string>();
            user.elo_rating = row["elo_rating"].as<int>();

            return user;

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return std::nullopt;
        }
    }

    // 사용자 생성
    bool create_user(const std::string& username, const std::string& email, const std::string& password_hash) {
        try {
            pqxx::work txn(conn_);

            txn.exec_params(
                "INSERT INTO users (username, email, password_hash) VALUES ($1, $2, $3)",
                username, email, password_hash
            );

            txn.commit();
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Error creating user: " << e.what() << "\n";
            return false;
        }
    }

    // ELO 업데이트
    bool update_elo(int user_id, int new_elo) {
        try {
            pqxx::work txn(conn_);

            txn.exec_params(
                "UPDATE users SET elo_rating = $1, updated_at = NOW() WHERE id = $2",
                new_elo, user_id
            );

            txn.commit();
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Error updating ELO: " << e.what() << "\n";
            return false;
        }
    }
};

int main() {
    UserRepository repo("host=localhost dbname=gamedb user=gameuser password=gamepass123");

    // 사용자 조회
    auto user = repo.find_by_username("alice");
    if (user) {
        std::cout << "User: " << user->username << ", ELO: " << user->elo_rating << "\n";
    }

    // ELO 업데이트
    if (user) {
        repo.update_elo(user->id, user->elo_rating + 50);
        std::cout << "✅ ELO updated\n";
    }

    return 0;
}
```

---

## 🔴 Part 3: Redis 기초 (20분)

### 3.1 Redis란?

**Redis**는 **인메모리 키-값 저장소**로, 게임 서버에서 세션 관리, 매치메이킹 큐, 리더보드 캐싱에 사용됩니다.

```
왜 Redis?
- 초고속 (메모리 기반)
- 데이터 구조 지원 (String, List, Set, Sorted Set, Hash)
- Pub/Sub 지원
- 간단한 API
```

### 3.2 Docker로 Redis 실행

**docker-compose.yml** (추가):
```yaml
services:
  # ... postgres 설정 ...

  redis:
    image: redis:7-alpine
    container_name: arena60-redis
    ports:
      - "6379:6379"
    command: redis-server --appendonly yes
    volumes:
      - redis_data:/data
    restart: unless-stopped

volumes:
  postgres_data:
  redis_data:
```

**실행**:
```bash
docker-compose up -d

# Redis 접속 확인
docker exec -it arena60-redis redis-cli

# 테스트
SET mykey "Hello Redis"
GET mykey

# 종료
exit
```

---

## 🔌 Part 4: hiredis - C++ Redis 클라이언트 (25분)

### 4.1 hiredis 설치

```bash
# Ubuntu/Debian
sudo apt-get install libhiredis-dev

# macOS
brew install hiredis

# 확인
pkg-config --modversion hiredis
```

### 4.2 간단한 연결 예제

**redis_example.cpp**:
```cpp
#include <iostream>
#include <hiredis/hiredis.h>
#include <memory>

class RedisClient {
private:
    std::unique_ptr<redisContext, decltype(&redisFree)> context_;

public:
    RedisClient(const std::string& host = "127.0.0.1", int port = 6379)
        : context_(nullptr, redisFree)
    {
        context_.reset(redisConnect(host.c_str(), port));

        if (context_ == nullptr || context_->err) {
            if (context_) {
                throw std::runtime_error(std::string("Redis error: ") + context_->errstr);
            } else {
                throw std::runtime_error("Failed to allocate Redis context");
            }
        }

        std::cout << "✅ Connected to Redis\n";
    }

    // SET 명령
    bool set(const std::string& key, const std::string& value) {
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(), "SET %s %s", key.c_str(), value.c_str())
        );

        if (!reply) {
            std::cerr << "Redis SET error\n";
            return false;
        }

        bool success = (reply->type == REDIS_REPLY_STATUS &&
                       std::string(reply->str) == "OK");
        freeReplyObject(reply);
        return success;
    }

    // GET 명령
    std::optional<std::string> get(const std::string& key) {
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(), "GET %s", key.c_str())
        );

        if (!reply) {
            return std::nullopt;
        }

        if (reply->type == REDIS_REPLY_STRING) {
            std::string value(reply->str);
            freeReplyObject(reply);
            return value;
        }

        freeReplyObject(reply);
        return std::nullopt;
    }

    // ZADD 명령 (Sorted Set - 리더보드용)
    bool zadd(const std::string& key, int score, const std::string& member) {
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(), "ZADD %s %d %s",
                        key.c_str(), score, member.c_str())
        );

        if (!reply) {
            return false;
        }

        bool success = (reply->type == REDIS_REPLY_INTEGER);
        freeReplyObject(reply);
        return success;
    }

    // ZREVRANGE 명령 (상위 N명 조회)
    std::vector<std::pair<std::string, int>> get_top_n(const std::string& key, int n) {
        std::vector<std::pair<std::string, int>> results;

        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(), "ZREVRANGE %s 0 %d WITHSCORES",
                        key.c_str(), n - 1)
        );

        if (!reply || reply->type != REDIS_REPLY_ARRAY) {
            if (reply) freeReplyObject(reply);
            return results;
        }

        for (size_t i = 0; i < reply->elements; i += 2) {
            std::string member = reply->element[i]->str;
            int score = std::stoi(reply->element[i + 1]->str);
            results.emplace_back(member, score);
        }

        freeReplyObject(reply);
        return results;
    }
};

int main() {
    try {
        RedisClient redis;

        // 간단한 키-값 저장
        redis.set("player:alice:session", "abc123");
        auto session = redis.get("player:alice:session");
        if (session) {
            std::cout << "Alice's session: " << *session << "\n";
        }

        // 리더보드 (Sorted Set)
        redis.zadd("leaderboard", 1300, "charlie");
        redis.zadd("leaderboard", 1200, "alice");
        redis.zadd("leaderboard", 1150, "bob");

        std::cout << "\n=== Top 3 Leaderboard ===\n";
        auto top3 = redis.get_top_n("leaderboard", 3);
        for (size_t i = 0; i < top3.size(); ++i) {
            std::cout << (i + 1) << ". " << top3[i].first
                      << ": " << top3[i].second << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.20)
project(redis_example)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(redis_example redis_example.cpp)
target_link_libraries(redis_example PRIVATE hiredis)
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./redis_example
```

**출력**:
```
✅ Connected to Redis
Alice's session: abc123

=== Top 3 Leaderboard ===
1. charlie: 1300
2. alice: 1200
3. bob: 1150
```

---

## 🎮 Part 5: 실전 통합 - 매치메이킹 큐 (20분)

### 5.1 Redis 매치메이킹 큐

```cpp
#include <hiredis/hiredis.h>
#include <iostream>
#include <string>
#include <memory>

class MatchmakingQueue {
private:
    std::unique_ptr<redisContext, decltype(&redisFree)> redis_;

public:
    MatchmakingQueue() : redis_(nullptr, redisFree) {
        redis_.reset(redisConnect("127.0.0.1", 6379));
        if (!redis_ || redis_->err) {
            throw std::runtime_error("Redis connection failed");
        }
    }

    // 큐에 플레이어 추가 (ELO 기반 정렬)
    bool enqueue(int user_id, int elo_rating) {
        auto reply = static_cast<redisReply*>(
            redisCommand(redis_.get(),
                "ZADD matchmaking_queue %d player:%d",
                elo_rating, user_id)
        );

        if (!reply) return false;

        bool success = (reply->type == REDIS_REPLY_INTEGER);
        freeReplyObject(reply);

        std::cout << "✅ Player " << user_id << " (ELO " << elo_rating
                  << ") joined queue\n";
        return success;
    }

    // 유사한 ELO의 플레이어 2명 찾기
    std::pair<int, int> find_match(int min_elo, int max_elo) {
        auto reply = static_cast<redisReply*>(
            redisCommand(redis_.get(),
                "ZRANGEBYSCORE matchmaking_queue %d %d LIMIT 0 2",
                min_elo, max_elo)
        );

        if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements < 2) {
            if (reply) freeReplyObject(reply);
            return {-1, -1};
        }

        // player:123 형식에서 ID 추출
        std::string p1_str = reply->element[0]->str;
        std::string p2_str = reply->element[1]->str;

        int p1_id = std::stoi(p1_str.substr(7));  // "player:" 이후
        int p2_id = std::stoi(p2_str.substr(7));

        // 큐에서 제거
        redisCommand(redis_.get(), "ZREM matchmaking_queue %s %s",
                    p1_str.c_str(), p2_str.c_str());

        freeReplyObject(reply);

        std::cout << "🎮 Match found: Player " << p1_id
                  << " vs Player " << p2_id << "\n";

        return {p1_id, p2_id};
    }

    // 큐 크기 조회
    int get_queue_size() {
        auto reply = static_cast<redisReply*>(
            redisCommand(redis_.get(), "ZCARD matchmaking_queue")
        );

        if (!reply || reply->type != REDIS_REPLY_INTEGER) {
            if (reply) freeReplyObject(reply);
            return 0;
        }

        int size = reply->integer;
        freeReplyObject(reply);
        return size;
    }
};

int main() {
    MatchmakingQueue queue;

    // 플레이어들이 큐에 참가
    queue.enqueue(1, 1200);
    queue.enqueue(2, 1220);
    queue.enqueue(3, 1180);
    queue.enqueue(4, 1500);

    std::cout << "\nQueue size: " << queue.get_queue_size() << "\n\n";

    // 매칭 시도 (1150~1250 범위)
    auto match = queue.find_match(1150, 1250);
    if (match.first != -1) {
        std::cout << "Players matched!\n";
    }

    std::cout << "\nRemaining in queue: " << queue.get_queue_size() << "\n";

    return 0;
}
```

---

## 🐳 Part 6: Docker Compose 완전판 (10분)

### 6.1 통합 docker-compose.yml

```yaml
version: '3.8'

services:
  postgres:
    image: postgres:15-alpine
    container_name: arena60-postgres
    environment:
      POSTGRES_DB: gamedb
      POSTGRES_USER: gameuser
      POSTGRES_PASSWORD: gamepass123
    ports:
      - "5432:5432"
    volumes:
      - postgres_data:/var/lib/postgresql/data
      - ./migrations:/docker-entrypoint-initdb.d
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U gameuser -d gamedb"]
      interval: 10s
      timeout: 5s
      retries: 5
    restart: unless-stopped

  redis:
    image: redis:7-alpine
    container_name: arena60-redis
    ports:
      - "6379:6379"
    command: redis-server --appendonly yes --maxmemory 256mb --maxmemory-policy allkeys-lru
    volumes:
      - redis_data:/data
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 10s
      timeout: 3s
      retries: 5
    restart: unless-stopped

volumes:
  postgres_data:
  redis_data:

networks:
  default:
    name: arena60-network
```

### 6.2 유용한 Docker 명령어

```bash
# 전체 시작
docker-compose up -d

# 로그 확인
docker-compose logs -f postgres
docker-compose logs -f redis

# 상태 확인
docker-compose ps

# PostgreSQL 접속
docker exec -it arena60-postgres psql -U gameuser -d gamedb

# Redis 접속
docker exec -it arena60-redis redis-cli

# 전체 중지
docker-compose down

# 데이터 삭제 (주의!)
docker-compose down -v
```

---

## 🐛 자주 막히는 부분

### 문제 1: "connection refused" 에러

```cpp
// ❌ Docker 내부에서는 localhost 대신 서비스명 사용
pqxx::connection conn("host=localhost ...");

// ✅ Docker 네트워크 사용
pqxx::connection conn("host=arena60-postgres ...");

// ✅ 또는 로컬 개발 시 localhost
pqxx::connection conn("host=127.0.0.1 ...");
```

### 문제 2: libpqxx 링크 오류

```cmake
# ❌ 잘못된 CMake 설정
target_link_libraries(myapp pqxx)

# ✅ 올바른 설정
find_package(PostgreSQL REQUIRED)
target_link_libraries(myapp PRIVATE pqxx PostgreSQL::PostgreSQL)
```

### 문제 3: Redis 메모리 부족

```bash
# Redis 메모리 제한 설정
docker-compose.yml:
  command: redis-server --maxmemory 256mb --maxmemory-policy allkeys-lru

# 메모리 사용량 확인
docker exec -it arena60-redis redis-cli INFO memory
```

### 문제 4: SQL Injection 취약점

```cpp
// ❌ 위험: 사용자 입력 직접 연결
std::string query = "SELECT * FROM users WHERE username = '" + username + "'";

// ✅ 안전: 파라미터화된 쿼리
txn.exec_params("SELECT * FROM users WHERE username = $1", username);
```

### 문제 5: Redis 연결 누수

```cpp
// ❌ 메모리 누수
redisContext* ctx = redisConnect("127.0.0.1", 6379);
// ... (redisFree 호출 안 함)

// ✅ RAII 패턴 사용
std::unique_ptr<redisContext, decltype(&redisFree)> ctx(
    redisConnect("127.0.0.1", 6379), redisFree
);
```

---

## ✅ 완료 체크리스트

### Part 1: PostgreSQL
- [ ] Docker로 PostgreSQL 실행
- [ ] 스키마 생성 및 샘플 데이터 삽입
- [ ] psql로 데이터 확인

### Part 2: libpqxx
- [ ] libpqxx 설치 및 확인
- [ ] 간단한 연결 예제 실행
- [ ] 파라미터화된 쿼리 작성

### Part 3: Redis
- [ ] Docker로 Redis 실행
- [ ] redis-cli로 명령 테스트

### Part 4: hiredis
- [ ] hiredis 설치 및 확인
- [ ] 간단한 연결 예제 실행
- [ ] Sorted Set으로 리더보드 구현

### Part 5: 실전 통합
- [ ] 매치메이킹 큐 구현
- [ ] ELO 기반 매칭 테스트

### Part 6: Docker Compose
- [ ] docker-compose.yml 작성
- [ ] 헬스 체크 확인
- [ ] 데이터 영속성 확인

---

## 🚀 다음 단계

✅ **PostgreSQL + Redis + Docker 완료!**

**다음 학습**:
- [**Quickstart 44**](44-elo-db-integration.md) - ELO + PostgreSQL 통합
- [**Quickstart 45**](45-matchmaking-system.md) - 매치메이킹 시스템
- [**Quickstart 70**](70-google-test.md) - 단위 테스트

**실전 적용**:
- MVP 1.2 - Matchmaking (Redis 큐)
- MVP 1.3 - Statistics & Ranking (PostgreSQL)

---

## 📚 참고 자료

- [PostgreSQL Documentation](https://www.postgresql.org/docs/)
- [libpqxx Tutorial](https://pqxx.org/development/libpqxx/)
- [Redis Documentation](https://redis.io/documentation)
- [hiredis GitHub](https://github.com/redis/hiredis)
- [Docker Compose Reference](https://docs.docker.com/compose/)

---

**Last Updated**: 2025-01-30
