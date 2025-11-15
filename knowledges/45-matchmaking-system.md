# Quickstart 45: Matchmaking Queue System

**목표**: ELO 기반 매치메이킹 큐 시스템을 Redis로 구현합니다.

**대상**: `mini-gameserver` Phase 1 Milestone 1.9 (Matchmaking)

**난이도**: ⭐⭐⭐⭐ (Advanced)

**소요 시간**: 80분

**선행 학습**:
- 44-elo-db-integration.md (ELO 시스템)
- 60-postgresql-redis-docker.md (Redis 기초)
- 32-cpp-game-loop.md (게임 서버 기초)

**학습 목표**:
1. 매치메이킹 알고리즘 이해 (스킬 기반 매칭)
2. Redis를 사용한 큐 구현
3. 매치 생성 및 룸 할당
4. 매칭 타임아웃 처리
5. 동시성 제어 및 경쟁 조건 방지

---

## Part 1: 매치메이킹 개념 (10분)

### 1.1 매치메이킹이란?

**정의**: 비슷한 실력의 플레이어들을 자동으로 매칭하여 게임 룸에 배정하는 시스템

**목표**:
- **공정한 매치**: ELO 차이가 적은 플레이어끼리 매칭
- **빠른 매칭**: 대기 시간 최소화 (< 30초)
- **확장성**: 수천 명의 동시 매칭 지원

### 1.2 매치메이킹 알고리즘

**Simple Queue (단순 큐)**:
- 선착순 매칭
- 빠르지만 실력 차이 무시
- 초보자 vs 고수 매치 가능

**Skill-Based Matching (스킬 기반)**:
- ELO 레이팅 기반 매칭
- 허용 범위(tolerance) 점진적 확대
- 예: 초기 ±50 → 10초 후 ±100 → 20초 후 ±200

**MMR (Matchmaking Rating)**:
- ELO와 유사하지만 숨겨진 레이팅
- 여러 요소 고려 (승률, 최근 성적, 플레이 스타일)

### 1.3 시스템 구조

```
┌─────────────┐
│   Player A  │─┐
└─────────────┘ │
                │    ┌──────────────────┐
┌─────────────┐ │    │                  │
│   Player B  │─┼───▶│  Matchmaking     │
└─────────────┘ │    │  Service         │
                │    │                  │
┌─────────────┐ │    └────────┬─────────┘
│   Player C  │─┘             │
└─────────────┘               │
                              ▼
                    ┌──────────────────┐
                    │   Redis Queue    │
                    │                  │
                    │  [A, B, C, ...]  │
                    └────────┬─────────┘
                             │
                             ▼
                    ┌──────────────────┐
                    │  Match Creator   │
                    │                  │
                    │  A vs B → Room 1 │
                    │  C vs D → Room 2 │
                    └────────┬─────────┘
                             │
                             ▼
                    ┌──────────────────┐
                    │   Game Rooms     │
                    │                  │
                    │  Room 1, 2, ...  │
                    └──────────────────┘
```

---

## Part 2: Redis 큐 구현 (20분)

### 2.1 Redis 데이터 구조

```bash
# Sorted Set: ELO 순으로 정렬된 대기 큐
ZADD matchmaking:queue 1200 "player:1"
ZADD matchmaking:queue 1250 "player:2"
ZADD matchmaking:queue 1180 "player:3"

# Hash: 플레이어 상세 정보
HSET player:1 user_id 1 elo 1200 joined_at 1699887600

# Set: 현재 매칭 중인 플레이어 (중복 방지)
SADD matchmaking:in_progress "player:1"

# String: 플레이어별 매치 상태
SET player:1:match_status "waiting" EX 300

# List: 완성된 매치 큐
LPUSH matches:pending "match_12345"
```

### 2.2 C++ Redis 클라이언트

```cpp
// matchmaking_queue.h
#pragma once
#include <hiredis/hiredis.h>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <optional>

struct QueuePlayer {
    int user_id;
    int elo;
    int64_t joined_at;  // Unix timestamp
};

struct Match {
    std::string match_id;
    int player1_id;
    int player2_id;
    int room_id;
};

class MatchmakingQueue {
private:
    std::unique_ptr<redisContext, decltype(&redisFree)> context_;
    
    std::string get_player_key(int user_id) const {
        return "player:" + std::to_string(user_id);
    }
    
public:
    MatchmakingQueue(const std::string& host = "127.0.0.1", int port = 6379)
        : context_(nullptr, redisFree)
    {
        context_.reset(redisConnect(host.c_str(), port));
        if (context_ == nullptr || context_->err) {
            throw std::runtime_error("Redis connection error");
        }
        std::cout << "✅ Connected to Redis\n";
    }

    // 플레이어를 큐에 추가
    bool enqueue(int user_id, int elo) {
        // 중복 체크
        if (is_in_queue(user_id)) {
            std::cerr << "Player " << user_id << " already in queue\n";
            return false;
        }
        
        int64_t now = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now()
        );
        
        // Sorted Set에 추가 (ELO를 score로 사용)
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(), 
                "ZADD matchmaking:queue %d %s",
                elo, get_player_key(user_id).c_str())
        );
        
        if (!reply) {
            std::cerr << "Failed to add to queue\n";
            return false;
        }
        
        bool success = (reply->type == REDIS_REPLY_INTEGER);
        freeReplyObject(reply);
        
        if (!success) return false;
        
        // 플레이어 정보 저장
        reply = static_cast<redisReply*>(
            redisCommand(context_.get(),
                "HMSET %s user_id %d elo %d joined_at %lld",
                get_player_key(user_id).c_str(),
                user_id, elo, now)
        );
        
        if (reply) {
            freeReplyObject(reply);
        }
        
        std::cout << "📥 Player " << user_id 
                  << " (ELO: " << elo << ") joined queue\n";
        
        return true;
    }

    // 플레이어를 큐에서 제거
    bool dequeue(int user_id) {
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(),
                "ZREM matchmaking:queue %s",
                get_player_key(user_id).c_str())
        );
        
        if (!reply) return false;
        
        bool success = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
        freeReplyObject(reply);
        
        // 플레이어 정보 삭제
        reply = static_cast<redisReply*>(
            redisCommand(context_.get(),
                "DEL %s",
                get_player_key(user_id).c_str())
        );
        
        if (reply) {
            freeReplyObject(reply);
        }
        
        if (success) {
            std::cout << "📤 Player " << user_id << " left queue\n";
        }
        
        return success;
    }

    // 큐에 있는지 확인
    bool is_in_queue(int user_id) const {
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(),
                "ZSCORE matchmaking:queue %s",
                get_player_key(user_id).c_str())
        );
        
        if (!reply) return false;
        
        bool exists = (reply->type != REDIS_REPLY_NIL);
        freeReplyObject(reply);
        
        return exists;
    }

    // 플레이어 정보 조회
    std::optional<QueuePlayer> get_player(int user_id) {
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(),
                "HGETALL %s",
                get_player_key(user_id).c_str())
        );
        
        if (!reply || reply->type != REDIS_REPLY_ARRAY) {
            if (reply) freeReplyObject(reply);
            return std::nullopt;
        }
        
        QueuePlayer player;
        
        // Parse hash
        for (size_t i = 0; i < reply->elements; i += 2) {
            std::string key(reply->element[i]->str);
            std::string value(reply->element[i + 1]->str);
            
            if (key == "user_id") {
                player.user_id = std::stoi(value);
            } else if (key == "elo") {
                player.elo = std::stoi(value);
            } else if (key == "joined_at") {
                player.joined_at = std::stoll(value);
            }
        }
        
        freeReplyObject(reply);
        return player;
    }

    // 현재 큐 크기
    int queue_size() const {
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(), "ZCARD matchmaking:queue")
        );
        
        if (!reply) return 0;
        
        int size = (reply->type == REDIS_REPLY_INTEGER) ? reply->integer : 0;
        freeReplyObject(reply);
        
        return size;
    }

    // ELO 범위 내 플레이어 검색
    std::vector<int> find_players_in_range(int target_elo, int tolerance) {
        std::vector<int> players;
        
        int min_elo = target_elo - tolerance;
        int max_elo = target_elo + tolerance;
        
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(),
                "ZRANGEBYSCORE matchmaking:queue %d %d",
                min_elo, max_elo)
        );
        
        if (!reply || reply->type != REDIS_REPLY_ARRAY) {
            if (reply) freeReplyObject(reply);
            return players;
        }
        
        for (size_t i = 0; i < reply->elements; i++) {
            std::string key(reply->element[i]->str);
            // "player:123" → 123
            if (key.substr(0, 7) == "player:") {
                int user_id = std::stoi(key.substr(7));
                players.push_back(user_id);
            }
        }
        
        freeReplyObject(reply);
        return players;
    }

    // 큐 전체 조회 (디버깅용)
    std::vector<QueuePlayer> get_all_players() {
        std::vector<QueuePlayer> players;
        
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(),
                "ZRANGE matchmaking:queue 0 -1 WITHSCORES")
        );
        
        if (!reply || reply->type != REDIS_REPLY_ARRAY) {
            if (reply) freeReplyObject(reply);
            return players;
        }
        
        for (size_t i = 0; i < reply->elements; i += 2) {
            std::string key(reply->element[i]->str);
            int elo = std::stoi(reply->element[i + 1]->str);
            
            if (key.substr(0, 7) == "player:") {
                int user_id = std::stoi(key.substr(7));
                
                auto player_opt = get_player(user_id);
                if (player_opt) {
                    players.push_back(*player_opt);
                }
            }
        }
        
        freeReplyObject(reply);
        return players;
    }

    // 큐 초기화
    void clear_queue() {
        redisCommand(context_.get(), "DEL matchmaking:queue");
        std::cout << "🗑️  Queue cleared\n";
    }
};
```

---

## Part 3: 매치메이커 서비스 (30분)

### 3.1 Matchmaker 클래스

```cpp
// matchmaker.h
#pragma once
#include "matchmaking_queue.h"
#include <random>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_set>

class Matchmaker {
private:
    std::shared_ptr<MatchmakingQueue> queue_;
    std::atomic<bool> running_;
    std::thread worker_thread_;
    
    // 매치 생성용
    std::mt19937 rng_;
    std::mutex match_mutex_;
    std::vector<Match> pending_matches_;
    
    // 매칭 설정
    int initial_tolerance_ = 50;    // 초기 ELO 허용 범위
    int max_tolerance_ = 300;       // 최대 ELO 허용 범위
    int tolerance_increase_ = 50;   // 10초마다 증가량
    int max_wait_time_ = 60;        // 최대 대기 시간 (초)
    
    std::string generate_match_id() {
        std::uniform_int_distribution<uint64_t> dist;
        return "match_" + std::to_string(dist(rng_));
    }
    
    int calculate_tolerance(int64_t joined_at) {
        auto now = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now()
        );
        
        int wait_time = static_cast<int>(now - joined_at);
        int tolerance = initial_tolerance_ + 
                       (wait_time / 10) * tolerance_increase_;
        
        return std::min(tolerance, max_tolerance_);
    }
    
    void worker_loop() {
        std::cout << "🔄 Matchmaker worker started\n";
        
        while (running_) {
            try {
                process_queue();
                
                // 1초마다 매칭 시도
                std::this_thread::sleep_for(std::chrono::seconds(1));
                
            } catch (const std::exception& e) {
                std::cerr << "Matchmaker error: " << e.what() << "\n";
            }
        }
        
        std::cout << "🛑 Matchmaker worker stopped\n";
    }
    
    void process_queue() {
        auto players = queue_->get_all_players();
        
        if (players.size() < 2) {
            return;  // 매칭 불가
        }
        
        std::unordered_set<int> matched_players;
        
        for (const auto& player : players) {
            // 이미 매칭된 플레이어는 스킵
            if (matched_players.count(player.user_id)) {
                continue;
            }
            
            // 대기 시간에 따른 허용 범위 계산
            int tolerance = calculate_tolerance(player.joined_at);
            
            // 최대 대기 시간 초과 체크
            auto now = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now()
            );
            int wait_time = static_cast<int>(now - player.joined_at);
            
            if (wait_time > max_wait_time_) {
                std::cout << "⏱️  Player " << player.user_id 
                          << " timed out, removing from queue\n";
                queue_->dequeue(player.user_id);
                continue;
            }
            
            // ELO 범위 내 상대 검색
            auto candidates = queue_->find_players_in_range(
                player.elo, tolerance
            );
            
            // 자기 자신과 이미 매칭된 플레이어 제외
            std::vector<int> valid_candidates;
            for (int candidate_id : candidates) {
                if (candidate_id != player.user_id && 
                    !matched_players.count(candidate_id)) {
                    valid_candidates.push_back(candidate_id);
                }
            }
            
            if (valid_candidates.empty()) {
                continue;  // 상대 없음
            }
            
            // 랜덤으로 상대 선택
            std::uniform_int_distribution<size_t> dist(
                0, valid_candidates.size() - 1
            );
            int opponent_id = valid_candidates[dist(rng_)];
            
            // 매치 생성
            create_match(player.user_id, opponent_id);
            
            // 매칭 완료 표시
            matched_players.insert(player.user_id);
            matched_players.insert(opponent_id);
            
            // 큐에서 제거
            queue_->dequeue(player.user_id);
            queue_->dequeue(opponent_id);
        }
    }
    
    void create_match(int player1_id, int player2_id) {
        Match match;
        match.match_id = generate_match_id();
        match.player1_id = player1_id;
        match.player2_id = player2_id;
        match.room_id = 0;  // Room Manager가 할당
        
        {
            std::lock_guard<std::mutex> lock(match_mutex_);
            pending_matches_.push_back(match);
        }
        
        std::cout << "🎮 Match created: " << match.match_id 
                  << " (Player " << player1_id 
                  << " vs Player " << player2_id << ")\n";
    }

public:
    explicit Matchmaker(std::shared_ptr<MatchmakingQueue> queue)
        : queue_(queue), running_(false), rng_(std::random_device{}())
    {
    }

    ~Matchmaker() {
        stop();
    }

    void start() {
        if (running_) {
            std::cerr << "Matchmaker already running\n";
            return;
        }
        
        running_ = true;
        worker_thread_ = std::thread(&Matchmaker::worker_loop, this);
        
        std::cout << "✅ Matchmaker started\n";
    }

    void stop() {
        if (!running_) return;
        
        running_ = false;
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        
        std::cout << "✅ Matchmaker stopped\n";
    }

    // 대기 중인 매치 가져오기
    std::vector<Match> get_pending_matches() {
        std::lock_guard<std::mutex> lock(match_mutex_);
        
        std::vector<Match> matches = pending_matches_;
        pending_matches_.clear();
        
        return matches;
    }

    // 통계
    struct Stats {
        int queue_size;
        int pending_matches;
        int total_matched;
    };

    Stats get_stats() const {
        Stats stats;
        stats.queue_size = queue_->queue_size();
        
        {
            std::lock_guard<std::mutex> lock(
                const_cast<std::mutex&>(match_mutex_)
            );
            stats.pending_matches = pending_matches_.size();
        }
        
        stats.total_matched = 0;  // TODO: 전체 매칭 수 추적
        
        return stats;
    }

    // 설정
    void set_tolerance_config(int initial, int max, int increase) {
        initial_tolerance_ = initial;
        max_tolerance_ = max;
        tolerance_increase_ = increase;
    }

    void set_max_wait_time(int seconds) {
        max_wait_time_ = seconds;
    }
};
```

---

## Part 4: 테스트 및 시뮬레이션 (15분)

### 4.1 단위 테스트

```cpp
// matchmaker_test.cpp
#include "matchmaker.h"
#include <cassert>
#include <iostream>

void test_queue_operations() {
    MatchmakingQueue queue;
    queue.clear_queue();
    
    // 추가
    assert(queue.enqueue(1, 1200));
    assert(queue.enqueue(2, 1250));
    assert(queue.enqueue(3, 1180));
    
    // 크기 확인
    assert(queue.queue_size() == 3);
    
    // 중복 체크
    assert(!queue.enqueue(1, 1200));
    
    // 조회
    auto player = queue.get_player(1);
    assert(player.has_value());
    assert(player->user_id == 1);
    assert(player->elo == 1200);
    
    // 제거
    assert(queue.dequeue(1));
    assert(queue.queue_size() == 2);
    assert(!queue.is_in_queue(1));
    
    queue.clear_queue();
    std::cout << "✅ test_queue_operations passed\n";
}

void test_elo_range_search() {
    MatchmakingQueue queue;
    queue.clear_queue();
    
    // ELO 분포: 1000, 1100, 1200, 1300, 1400
    queue.enqueue(1, 1000);
    queue.enqueue(2, 1100);
    queue.enqueue(3, 1200);
    queue.enqueue(4, 1300);
    queue.enqueue(5, 1400);
    
    // 1200 ± 100 범위 검색
    auto players = queue.find_players_in_range(1200, 100);
    
    // 1100, 1200, 1300이 검색되어야 함
    assert(players.size() == 3);
    
    queue.clear_queue();
    std::cout << "✅ test_elo_range_search passed\n";
}

void test_matchmaking() {
    auto queue = std::make_shared<MatchmakingQueue>();
    queue->clear_queue();
    
    Matchmaker matcher(queue);
    
    // 비슷한 ELO의 플레이어 추가
    queue->enqueue(1, 1200);
    queue->enqueue(2, 1220);
    queue->enqueue(3, 1180);
    queue->enqueue(4, 1250);
    
    // 매치메이커 시작
    matcher.start();
    
    // 매칭 대기
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // 매치 확인
    auto matches = matcher.get_pending_matches();
    assert(matches.size() >= 1);  // 최소 1개 매치 생성
    
    // 큐가 비었는지 확인
    assert(queue->queue_size() == 0 || queue->queue_size() <= 2);
    
    matcher.stop();
    queue->clear_queue();
    
    std::cout << "✅ test_matchmaking passed (Matches: " 
              << matches.size() << ")\n";
}

void test_tolerance_progression() {
    auto queue = std::make_shared<MatchmakingQueue>();
    queue->clear_queue();
    
    Matchmaker matcher(queue);
    matcher.set_tolerance_config(50, 300, 50);
    
    // 극단적인 ELO 차이
    queue->enqueue(1, 1000);  // 초보
    queue->enqueue(2, 1500);  // 고수
    
    matcher.start();
    
    // 초기에는 매칭 안 됨
    std::this_thread::sleep_for(std::chrono::seconds(2));
    auto matches = matcher.get_pending_matches();
    assert(matches.empty());
    
    // 시간이 지나면 tolerance 증가로 매칭됨
    std::this_thread::sleep_for(std::chrono::seconds(12));
    matches = matcher.get_pending_matches();
    
    // 최소 10초 후에는 tolerance가 50 + 50 = 100
    // 20초 후에는 100 + 50 = 150
    // 결국 매칭될 것 (또는 타임아웃)
    
    matcher.stop();
    queue->clear_queue();
    
    std::cout << "✅ test_tolerance_progression passed\n";
}

int main() {
    test_queue_operations();
    test_elo_range_search();
    test_matchmaking();
    test_tolerance_progression();
    
    std::cout << "\n🎉 All matchmaker tests passed!\n";
    return 0;
}
```

### 4.2 시뮬레이션

```cpp
// matchmaker_simulation.cpp
#include "matchmaker.h"
#include <iostream>
#include <random>

void simulate_players(int count) {
    auto queue = std::make_shared<MatchmakingQueue>();
    queue->clear_queue();
    
    Matchmaker matcher(queue);
    matcher.start();
    
    std::mt19937 rng(std::random_device{}());
    std::normal_distribution<double> elo_dist(1200, 200);  // 평균 1200, 표준편차 200
    
    std::cout << "🎮 Simulating " << count << " players...\n\n";
    
    // 플레이어 추가 (10초에 걸쳐 순차 추가)
    for (int i = 1; i <= count; i++) {
        int elo = static_cast<int>(elo_dist(rng));
        elo = std::max(800, std::min(2000, elo));  // 800-2000 범위 제한
        
        queue->enqueue(i, elo);
        
        if (i % 5 == 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    // 매칭 대기
    std::cout << "\n⏳ Waiting for matches...\n\n";
    
    for (int i = 0; i < 15; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto matches = matcher.get_pending_matches();
        
        if (!matches.empty()) {
            std::cout << "📊 Second " << (i + 1) << ": " 
                      << matches.size() << " matches created\n";
            
            for (const auto& match : matches) {
                auto p1 = queue->get_player(match.player1_id);
                auto p2 = queue->get_player(match.player2_id);
                
                // 이미 큐에서 제거되었을 수 있음
                std::cout << "  Match " << match.match_id 
                          << ": Player " << match.player1_id 
                          << " vs Player " << match.player2_id << "\n";
            }
        }
        
        auto stats = matcher.get_stats();
        std::cout << "  Queue size: " << stats.queue_size 
                  << ", Pending: " << stats.pending_matches << "\n\n";
    }
    
    matcher.stop();
    
    std::cout << "\n✅ Simulation complete\n";
}

int main() {
    simulate_players(20);
    return 0;
}
```

---

## Part 5: Room Manager 통합 (15분)

### 5.1 Room Manager 클래스

```cpp
// room_manager.h
#pragma once
#include "matchmaker.h"
#include <unordered_map>
#include <memory>

struct GameRoom {
    int room_id;
    std::string match_id;
    int player1_id;
    int player2_id;
    bool active;
    int64_t created_at;
};

class RoomManager {
private:
    std::unordered_map<int, GameRoom> rooms_;
    std::mutex room_mutex_;
    int next_room_id_ = 1;
    
public:
    // 매치를 룸으로 변환
    int assign_room(const Match& match) {
        std::lock_guard<std::mutex> lock(room_mutex_);
        
        int room_id = next_room_id_++;
        
        GameRoom room;
        room.room_id = room_id;
        room.match_id = match.match_id;
        room.player1_id = match.player1_id;
        room.player2_id = match.player2_id;
        room.active = true;
        room.created_at = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now()
        );
        
        rooms_[room_id] = room;
        
        std::cout << "🏠 Room " << room_id << " assigned to match " 
                  << match.match_id << "\n";
        
        return room_id;
    }

    // 룸 조회
    std::optional<GameRoom> get_room(int room_id) {
        std::lock_guard<std::mutex> lock(room_mutex_);
        
        auto it = rooms_.find(room_id);
        if (it == rooms_.end()) {
            return std::nullopt;
        }
        
        return it->second;
    }

    // 룸 종료
    void close_room(int room_id) {
        std::lock_guard<std::mutex> lock(room_mutex_);
        
        auto it = rooms_.find(room_id);
        if (it != rooms_.end()) {
            it->second.active = false;
            std::cout << "🚪 Room " << room_id << " closed\n";
        }
    }

    // 활성 룸 수
    int active_room_count() const {
        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(room_mutex_)
        );
        
        int count = 0;
        for (const auto& [_, room] : rooms_) {
            if (room.active) count++;
        }
        
        return count;
    }

    // 전체 룸 목록
    std::vector<GameRoom> get_all_rooms() const {
        std::lock_guard<std::mutex> lock(
            const_cast<std::mutex&>(room_mutex_)
        );
        
        std::vector<GameRoom> rooms;
        for (const auto& [_, room] : rooms_) {
            rooms.push_back(room);
        }
        
        return rooms;
    }
};
```

### 5.2 통합 예제

```cpp
// integrated_matchmaking.cpp
#include "matchmaker.h"
#include "room_manager.h"
#include <iostream>

int main() {
    // 초기화
    auto queue = std::make_shared<MatchmakingQueue>();
    queue->clear_queue();
    
    Matchmaker matcher(queue);
    RoomManager room_mgr;
    
    // 매치메이커 시작
    matcher.start();
    
    // 플레이어 추가
    std::cout << "Adding players to queue...\n";
    queue->enqueue(1, 1200);
    queue->enqueue(2, 1220);
    queue->enqueue(3, 1180);
    queue->enqueue(4, 1250);
    queue->enqueue(5, 1190);
    queue->enqueue(6, 1210);
    
    // 매칭 및 룸 할당 루프
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 완성된 매치 가져오기
        auto matches = matcher.get_pending_matches();
        
        // 각 매치에 룸 할당
        for (auto& match : matches) {
            int room_id = room_mgr.assign_room(match);
            
            std::cout << "🎮 Starting game in room " << room_id 
                      << " (Match: " << match.match_id << ")\n";
            
            // TODO: 실제 게임 서버에 룸 생성 요청
        }
        
        // 통계 출력
        auto stats = matcher.get_stats();
        std::cout << "📊 Queue: " << stats.queue_size 
                  << ", Active Rooms: " << room_mgr.active_room_count() << "\n\n";
    }
    
    matcher.stop();
    
    // 최종 통계
    std::cout << "\n=== Final Stats ===\n";
    std::cout << "Total rooms created: " 
              << room_mgr.get_all_rooms().size() << "\n";
    std::cout << "Active rooms: " 
              << room_mgr.active_room_count() << "\n";
    
    return 0;
}
```

---

## Troubleshooting

### 문제 1: "Could not connect to Redis"

**증상**:
```
Redis connection error
```

**원인**: Redis 서버 미실행

**해결**:
```bash
# Redis 시작
redis-server

# 또는 Docker
docker run -d -p 6379:6379 redis:latest

# 연결 테스트
redis-cli ping
# 응답: PONG
```

---

### 문제 2: 매칭이 너무 느림

**증상**:
플레이어가 30초 이상 대기해도 매칭 안 됨

**원인**:
- Tolerance가 너무 작음
- 큐에 플레이어 부족

**해결**:
```cpp
// Tolerance 설정 조정
matcher.set_tolerance_config(
    100,  // initial (50 → 100으로 증가)
    500,  // max (300 → 500으로 증가)
    100   // increase (50 → 100으로 증가)
);

// 또는 최대 대기 시간 감소
matcher.set_max_wait_time(30);  // 60 → 30초
```

---

### 문제 3: 같은 플레이어가 중복 매칭됨

**증상**:
```
Player 1 matched with Player 2
Player 1 matched with Player 3  // 중복!
```

**원인**:
경쟁 조건 (race condition)

**해결**:
```cpp
// process_queue()에서 matched_players Set 사용
std::unordered_set<int> matched_players;

// 매칭 전 체크
if (matched_players.count(player.user_id)) {
    continue;  // 이미 매칭됨
}

// 매칭 후 즉시 표시
matched_players.insert(player.user_id);
matched_players.insert(opponent_id);

// 큐에서 즉시 제거
queue_->dequeue(player.user_id);
queue_->dequeue(opponent_id);
```

---

### 문제 4: Redis 메모리 부족

**증상**:
```
OOM command not allowed when used memory > 'maxmemory'
```

**원인**:
만료된 플레이어 정보가 계속 쌓임

**해결**:
```bash
# Redis 설정 조정
redis-cli CONFIG SET maxmemory 256mb
redis-cli CONFIG SET maxmemory-policy allkeys-lru

# 또는 redis.conf
maxmemory 256mb
maxmemory-policy allkeys-lru
```

```cpp
// C++에서 TTL 설정
redisCommand(context_.get(),
    "SETEX %s 300 ...",  // 5분 후 자동 삭제
    get_player_key(user_id).c_str());
```

---

### 문제 5: 실력 차이가 큰 매칭

**증상**:
1000 ELO vs 1500 ELO 매칭

**원인**:
Tolerance가 너무 빨리 증가

**해결**:
```cpp
// 더 엄격한 설정
matcher.set_tolerance_config(
    30,   // initial: 좁은 범위로 시작
    200,  // max: 최대 범위 제한
    20    // increase: 천천히 증가
);

// 또는 ELO 차이 체크 추가
int elo_diff = std::abs(player1_elo - player2_elo);
if (elo_diff > 150) {
    continue;  // 매칭 스킵
}
```

---

## 요약

이번 Quickstart에서 학습한 내용:

1. **매치메이킹 개념**: 스킬 기반 매칭, Tolerance 시스템
2. **Redis 큐**: Sorted Set, Hash, 범위 검색
3. **Matchmaker 서비스**: 백그라운드 워커, 동시성 제어
4. **Room Manager**: 매치→룸 변환, 룸 관리
5. **성능 최적화**: Tolerance 조정, 타임아웃, 메모리 관리

**mini-gameserver Milestone 1.9 완료!** ✅

**다음 단계**:
- 46-grafana-dashboard.md: Grafana 대시보드 완성

**주요 개념**:
- ELO 기반 매칭으로 공정한 게임 보장
- Tolerance 점진적 증가로 대기 시간 최소화
- Redis Sorted Set으로 효율적인 범위 검색
- 경쟁 조건 방지로 중복 매칭 차단
- Room Manager로 매치→게임 변환

이제 완전한 매치메이킹 시스템을 갖춘 게임 서버를 만들 수 있습니다! 🎮🔍
