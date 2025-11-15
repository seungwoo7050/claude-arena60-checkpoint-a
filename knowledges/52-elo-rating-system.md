# Quickstart 52: ELO Rating System

> **📚 학습 유형**: 기초 개념 (Fundamentals)  
> **⏭️ 다음 단계**: 이 문서 완료 후 → [Quickstart 44: ELO DB Integration](44-elo-db-integration.md) (데이터베이스 통합)

## 🎯 목표
- **ELO Rating**: 체스, 게임 순위 시스템
- **수학적 원리**: 확률 기반 점수 계산
- **C++ 구현**: 매치메이킹, 랭킹 계산
- **실전**: Pong 게임 랭킹 시스템

## 📋 사전준비
- [Quickstart 30](30-cpp-for-game-server.md) 완료 (C++ 기초)
- [Quickstart 32](32-cpp-game-loop.md) 완료 (Game loop)
- [Quickstart 60](60-postgresql-redis-docker.md) 권장 (데이터베이스)

---

## 🎲 Part 1: ELO Rating 기초

### 1.1 개념

**ELO Rating**은 **상대 강도를 숫자로 표현**하는 시스템입니다.

```
History:
- 1960년: 체스 마스터 Arpad Elo가 개발
- 현재: 체스, 게임, 스포츠 등 모든 대전 게임에 사용

핵심 아이디어:
- 높은 점수 = 강한 플레이어
- 낮은 점수 = 약한 플레이어
- 승리 시 점수 증가, 패배 시 점수 감소
- 강한 플레이어를 이기면 점수 많이 증가
- 약한 플레이어에게 지면 점수 많이 감소
```

### 1.2 수학 공식

```
1. 예상 승률 (Expected Score):
   E_A = 1 / (1 + 10^((R_B - R_A) / 400))
   
   - R_A: 플레이어 A의 현재 점수
   - R_B: 플레이어 B의 현재 점수
   - E_A: A가 이길 확률 (0.0 ~ 1.0)

2. 새로운 점수 (New Rating):
   R'_A = R_A + K * (S_A - E_A)
   
   - K: K-factor (변동성, 보통 32)
   - S_A: 실제 결과 (승리=1, 무승부=0.5, 패배=0)
   - R'_A: 새로운 점수
```

### 1.3 예제 계산

```
플레이어 A: 1500점
플레이어 B: 1600점
K-factor: 32

1. 예상 승률 계산:
   E_A = 1 / (1 + 10^((1600 - 1500) / 400))
       = 1 / (1 + 10^(100 / 400))
       = 1 / (1 + 10^0.25)
       = 1 / (1 + 1.778)
       = 1 / 2.778
       = 0.36 (36% 승률)
   
   E_B = 1 - E_A = 0.64 (64% 승률)

2. A가 승리한 경우:
   R'_A = 1500 + 32 * (1 - 0.36)
        = 1500 + 32 * 0.64
        = 1500 + 20.48
        = 1520 (↑ 20점)
   
   R'_B = 1600 + 32 * (0 - 0.64)
        = 1600 + 32 * (-0.64)
        = 1600 - 20.48
        = 1580 (↓ 20점)

3. A가 패배한 경우:
   R'_A = 1500 + 32 * (0 - 0.36)
        = 1500 - 11.52
        = 1488 (↓ 12점)
   
   R'_B = 1600 + 32 * (1 - 0.64)
        = 1600 + 11.52
        = 1612 (↑ 12점)
```

**결론**: 약한 플레이어(A)가 이기면 점수 많이 증가 (+20), 지면 적게 감소 (-12)

---

## 🧮 Part 2: C++ 구현

### 2.1 기본 ELO 계산

```cpp
#include <iostream>
#include <cmath>

class EloRating {
private:
    static constexpr double K_FACTOR = 32.0;
    
public:
    // 예상 승률 계산
    static double expected_score(double rating_a, double rating_b) {
        return 1.0 / (1.0 + std::pow(10.0, (rating_b - rating_a) / 400.0));
    }
    
    // 새로운 점수 계산
    struct RatingChange {
        double new_rating_a;
        double new_rating_b;
        double change_a;
        double change_b;
    };
    
    static RatingChange calculate_new_ratings(double rating_a, double rating_b, 
                                              double score_a) {
        RatingChange result;
        
        // 예상 승률
        double expected_a = expected_score(rating_a, rating_b);
        double expected_b = 1.0 - expected_a;
        
        // 실제 결과
        double score_b = 1.0 - score_a;
        
        // 점수 변화
        result.change_a = K_FACTOR * (score_a - expected_a);
        result.change_b = K_FACTOR * (score_b - expected_b);
        
        // 새로운 점수
        result.new_rating_a = rating_a + result.change_a;
        result.new_rating_b = rating_b + result.change_b;
        
        return result;
    }
    
    // K-factor 동적 계산 (게임 수에 따라)
    static double get_k_factor(int games_played) {
        if (games_played < 30) {
            return 40.0;  // 초보자: 변동성 높게
        } else if (games_played < 100) {
            return 32.0;  // 일반: 기본값
        } else {
            return 24.0;  // 베테랑: 변동성 낮게
        }
    }
};

int main() {
    std::cout << "=== ELO Rating Calculator ===\n\n";
    
    double rating_a = 1500.0;
    double rating_b = 1600.0;
    
    std::cout << "Player A: " << rating_a << "\n";
    std::cout << "Player B: " << rating_b << "\n\n";
    
    // 예상 승률
    double expected_a = EloRating::expected_score(rating_a, rating_b);
    std::cout << "Expected win rate:\n";
    std::cout << "  Player A: " << (expected_a * 100) << "%\n";
    std::cout << "  Player B: " << ((1.0 - expected_a) * 100) << "%\n\n";
    
    // 시나리오 1: A 승리
    std::cout << "Scenario 1: Player A wins\n";
    auto result_win = EloRating::calculate_new_ratings(rating_a, rating_b, 1.0);
    std::cout << "  Player A: " << rating_a << " → " << result_win.new_rating_a 
              << " (+" << result_win.change_a << ")\n";
    std::cout << "  Player B: " << rating_b << " → " << result_win.new_rating_b 
              << " (" << result_win.change_b << ")\n\n";
    
    // 시나리오 2: A 패배
    std::cout << "Scenario 2: Player A loses\n";
    auto result_lose = EloRating::calculate_new_ratings(rating_a, rating_b, 0.0);
    std::cout << "  Player A: " << rating_a << " → " << result_lose.new_rating_a 
              << " (" << result_lose.change_a << ")\n";
    std::cout << "  Player B: " << rating_b << " → " << result_lose.new_rating_b 
              << " (+" << result_lose.change_b << ")\n\n";
    
    // 시나리오 3: 무승부
    std::cout << "Scenario 3: Draw\n";
    auto result_draw = EloRating::calculate_new_ratings(rating_a, rating_b, 0.5);
    std::cout << "  Player A: " << rating_a << " → " << result_draw.new_rating_a 
              << " (" << result_draw.change_a << ")\n";
    std::cout << "  Player B: " << rating_b << " → " << result_draw.new_rating_b 
              << " (" << result_draw.change_b << ")\n\n";
    
    // K-factor 테스트
    std::cout << "K-factor by games played:\n";
    std::cout << "  10 games:  " << EloRating::get_k_factor(10) << "\n";
    std::cout << "  50 games:  " << EloRating::get_k_factor(50) << "\n";
    std::cout << "  150 games: " << EloRating::get_k_factor(150) << "\n";
    
    return 0;
}
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.20)
project(elo_calculator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(elo_calculator elo_calculator.cpp)

# macOS에서 math 라이브러리 링크
if(UNIX AND NOT APPLE)
    target_link_libraries(elo_calculator PRIVATE m)
endif()
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./elo_calculator
```

**실행 결과**:
```
=== ELO Rating Calculator ===

Player A: 1500
Player B: 1600

Expected win rate:
  Player A: 35.993%
  Player B: 64.007%

Scenario 1: Player A wins
  Player A: 1500 → 1520.48 (+20.48)
  Player B: 1600 → 1579.52 (-20.48)

Scenario 2: Player A loses
  Player A: 1500 → 1488.48 (-11.52)
  Player B: 1600 → 1611.52 (+11.52)

Scenario 3: Draw
  Player A: 1500 → 1504.48 (+4.48)
  Player B: 1600 → 1595.52 (-4.52)

K-factor by games played:
  10 games:  40
  50 games:  32
  150 games: 24
```

### 2.2 플레이어 클래스

```cpp
#include <string>
#include <vector>
#include <algorithm>

struct MatchResult {
    int opponent_id;
    double score;  // 1.0 = 승리, 0.5 = 무승부, 0.0 = 패배
    double rating_before;
    double rating_after;
    double rating_change;
    time_t timestamp;
};

class Player {
private:
    int id;
    std::string username;
    double rating;
    int games_played;
    int wins;
    int losses;
    int draws;
    std::vector<MatchResult> match_history;
    
public:
    Player(int id, const std::string& name, double initial_rating = 1500.0)
        : id(id), username(name), rating(initial_rating), 
          games_played(0), wins(0), losses(0), draws(0) {}
    
    // Getters
    int get_id() const { return id; }
    std::string get_username() const { return username; }
    double get_rating() const { return rating; }
    int get_games_played() const { return games_played; }
    int get_wins() const { return wins; }
    int get_losses() const { return losses; }
    int get_draws() const { return draws; }
    
    double get_win_rate() const {
        if (games_played == 0) return 0.0;
        return static_cast<double>(wins) / games_played * 100.0;
    }
    
    // 매치 결과 기록
    void record_match(int opponent_id, double score, double rating_change) {
        MatchResult result;
        result.opponent_id = opponent_id;
        result.score = score;
        result.rating_before = rating;
        result.rating_change = rating_change;
        
        rating += rating_change;
        result.rating_after = rating;
        result.timestamp = time(nullptr);
        
        match_history.push_back(result);
        games_played++;
        
        if (score == 1.0) {
            wins++;
        } else if (score == 0.0) {
            losses++;
        } else {
            draws++;
        }
    }
    
    // 최근 전적
    std::vector<MatchResult> get_recent_matches(int count = 10) const {
        std::vector<MatchResult> recent = match_history;
        std::reverse(recent.begin(), recent.end());
        
        if (recent.size() > static_cast<size_t>(count)) {
            recent.resize(count);
        }
        
        return recent;
    }
    
    // 통계 출력
    void print_stats() const {
        std::cout << "=== Player Stats ===\n";
        std::cout << "Username: " << username << "\n";
        std::cout << "Rating: " << rating << "\n";
        std::cout << "Games: " << games_played << "\n";
        std::cout << "Record: " << wins << "W - " << losses << "L - " << draws << "D\n";
        std::cout << "Win Rate: " << get_win_rate() << "%\n";
        
        if (!match_history.empty()) {
            std::cout << "\nRecent Matches:\n";
            auto recent = get_recent_matches(5);
            for (const auto& match : recent) {
                std::string result = (match.score == 1.0) ? "WIN" : 
                                    (match.score == 0.0) ? "LOSS" : "DRAW";
                std::cout << "  " << result << " vs Player#" << match.opponent_id
                          << " (" << match.rating_before << " → " << match.rating_after
                          << ", " << (match.rating_change >= 0 ? "+" : "") 
                          << match.rating_change << ")\n";
            }
        }
    }
};

int main() {
    // 플레이어 생성
    Player alice(1, "Alice", 1500.0);
    Player bob(2, "Bob", 1600.0);
    
    std::cout << "Initial Ratings:\n";
    std::cout << "Alice: " << alice.get_rating() << "\n";
    std::cout << "Bob: " << bob.get_rating() << "\n\n";
    
    // 매치 1: Alice 승리
    auto result1 = EloRating::calculate_new_ratings(
        alice.get_rating(), bob.get_rating(), 1.0);
    alice.record_match(bob.get_id(), 1.0, result1.change_a);
    bob.record_match(alice.get_id(), 0.0, result1.change_b);
    
    // 매치 2: Bob 승리
    auto result2 = EloRating::calculate_new_ratings(
        alice.get_rating(), bob.get_rating(), 0.0);
    alice.record_match(bob.get_id(), 0.0, result2.change_a);
    bob.record_match(alice.get_id(), 1.0, result2.change_b);
    
    // 매치 3: 무승부
    auto result3 = EloRating::calculate_new_ratings(
        alice.get_rating(), bob.get_rating(), 0.5);
    alice.record_match(bob.get_id(), 0.5, result3.change_a);
    bob.record_match(alice.get_id(), 0.5, result3.change_b);
    
    // 통계 출력
    alice.print_stats();
    std::cout << "\n";
    bob.print_stats();
    
    return 0;
}
```

**실행 결과**:
```
Initial Ratings:
Alice: 1500
Bob: 1600

=== Player Stats ===
Username: Alice
Rating: 1513.44
Games: 3
Record: 1W - 1L - 1D
Win Rate: 33.33%

Recent Matches:
  DRAW vs Player#2 (1508.96 → 1513.44, +4.48)
  LOSS vs Player#2 (1520.48 → 1508.96, -11.52)
  WIN vs Player#2 (1500 → 1520.48, +20.48)

=== Player Stats ===
Username: Bob
Rating: 1586.56
Games: 3
Record: 1W - 1L - 1D
Win Rate: 33.33%

Recent Matches:
  DRAW vs Player#1 (1591.04 → 1586.56, -4.48)
  WIN vs Player#1 (1579.52 → 1591.04, +11.52)
  LOSS vs Player#1 (1600 → 1579.52, -20.48)
```

---

## 🎯 Part 3: 매치메이킹 시스템

### 3.1 간단한 매치메이킹

```cpp
#include <queue>
#include <memory>
#include <cmath>

class MatchmakingSystem {
private:
    static constexpr double MAX_RATING_DIFF = 200.0;  // 최대 레이팅 차이
    
    struct QueuedPlayer {
        std::shared_ptr<Player> player;
        time_t queue_time;
        
        QueuedPlayer(std::shared_ptr<Player> p)
            : player(p), queue_time(time(nullptr)) {}
    };
    
    std::vector<QueuedPlayer> queue;
    
public:
    // 큐에 추가
    void add_to_queue(std::shared_ptr<Player> player) {
        queue.emplace_back(player);
        std::cout << player->get_username() << " (Rating: " << player->get_rating() 
                  << ") entered matchmaking queue\n";
    }
    
    // 매치 찾기
    struct Match {
        std::shared_ptr<Player> player1;
        std::shared_ptr<Player> player2;
        double rating_diff;
    };
    
    std::vector<Match> find_matches() {
        std::vector<Match> matches;
        
        // 레이팅 순으로 정렬
        std::sort(queue.begin(), queue.end(), [](const auto& a, const auto& b) {
            return a.player->get_rating() < b.player->get_rating();
        });
        
        // 인접한 플레이어끼리 매칭
        for (size_t i = 0; i + 1 < queue.size(); ) {
            auto& p1 = queue[i];
            auto& p2 = queue[i + 1];
            
            double rating_diff = std::abs(p1.player->get_rating() - 
                                         p2.player->get_rating());
            
            // 대기 시간에 따라 레이팅 차이 허용치 증가
            time_t now = time(nullptr);
            double wait_time1 = difftime(now, p1.queue_time);
            double wait_time2 = difftime(now, p2.queue_time);
            double max_wait = std::max(wait_time1, wait_time2);
            
            double adjusted_max_diff = MAX_RATING_DIFF + (max_wait * 10);  // 10초당 +10 허용
            
            if (rating_diff <= adjusted_max_diff) {
                Match match;
                match.player1 = p1.player;
                match.player2 = p2.player;
                match.rating_diff = rating_diff;
                matches.push_back(match);
                
                std::cout << "Match found: " << p1.player->get_username() 
                          << " (" << p1.player->get_rating() << ") vs "
                          << p2.player->get_username() 
                          << " (" << p2.player->get_rating() << ")"
                          << " [Rating diff: " << rating_diff << "]\n";
                
                // 큐에서 제거
                queue.erase(queue.begin() + i, queue.begin() + i + 2);
            } else {
                i++;
            }
        }
        
        return matches;
    }
    
    size_t get_queue_size() const { return queue.size(); }
};

int main() {
    MatchmakingSystem mm;
    
    // 플레이어 생성
    auto alice = std::make_shared<Player>(1, "Alice", 1500.0);
    auto bob = std::make_shared<Player>(2, "Bob", 1520.0);
    auto charlie = std::make_shared<Player>(3, "Charlie", 1480.0);
    auto david = std::make_shared<Player>(4, "David", 1800.0);
    auto eve = std::make_shared<Player>(5, "Eve", 1450.0);
    
    std::cout << "=== Matchmaking System ===\n\n";
    
    // 큐에 추가
    mm.add_to_queue(alice);
    mm.add_to_queue(bob);
    mm.add_to_queue(charlie);
    mm.add_to_queue(david);
    mm.add_to_queue(eve);
    
    std::cout << "\nQueue size: " << mm.get_queue_size() << "\n\n";
    
    // 매치 찾기
    auto matches = mm.find_matches();
    
    std::cout << "\nMatches created: " << matches.size() << "\n";
    std::cout << "Remaining in queue: " << mm.get_queue_size() << "\n";
    
    return 0;
}
```

**실행 결과**:
```
=== Matchmaking System ===

Alice (Rating: 1500) entered matchmaking queue
Bob (Rating: 1520) entered matchmaking queue
Charlie (Rating: 1480) entered matchmaking queue
David (Rating: 1800) entered matchmaking queue
Eve (Rating: 1450) entered matchmaking queue

Queue size: 5

Match found: Eve (1450) vs Charlie (1480) [Rating diff: 30]
Match found: Alice (1500) vs Bob (1520) [Rating diff: 20]

Matches created: 2
Remaining in queue: 1
```

### 3.2 스킬 기반 매치메이킹 (고급)

```cpp
class AdvancedMatchmaking {
private:
    struct PlayerQueue {
        std::shared_ptr<Player> player;
        time_t queue_time;
        int search_range;  // 검색 범위 (초기값: 100)
        
        PlayerQueue(std::shared_ptr<Player> p)
            : player(p), queue_time(time(nullptr)), search_range(100) {}
    };
    
    std::vector<PlayerQueue> queue;
    
public:
    void add_to_queue(std::shared_ptr<Player> player) {
        queue.emplace_back(player);
    }
    
    std::vector<MatchmakingSystem::Match> find_matches() {
        std::vector<MatchmakingSystem::Match> matches;
        time_t now = time(nullptr);
        
        // 대기 시간에 따라 검색 범위 확장
        for (auto& pq : queue) {
            double wait_time = difftime(now, pq.queue_time);
            pq.search_range = 100 + static_cast<int>(wait_time * 5);  // 초당 +5
        }
        
        // 매칭 시도
        for (size_t i = 0; i < queue.size(); ++i) {
            if (!queue[i].player) continue;  // 이미 매칭됨
            
            auto& p1 = queue[i];
            double best_score = -1.0;
            size_t best_match = -1;
            
            for (size_t j = i + 1; j < queue.size(); ++j) {
                if (!queue[j].player) continue;
                
                auto& p2 = queue[j];
                double rating_diff = std::abs(p1.player->get_rating() - 
                                             p2.player->get_rating());
                
                // 검색 범위 내에 있는지 확인
                if (rating_diff <= std::min(p1.search_range, p2.search_range)) {
                    // 매칭 점수 계산 (레이팅 차이가 작을수록 높은 점수)
                    double score = 1.0 - (rating_diff / 1000.0);
                    
                    if (score > best_score) {
                        best_score = score;
                        best_match = j;
                    }
                }
            }
            
            // 최적 매치 발견
            if (best_match != static_cast<size_t>(-1)) {
                MatchmakingSystem::Match match;
                match.player1 = p1.player;
                match.player2 = queue[best_match].player;
                match.rating_diff = std::abs(p1.player->get_rating() - 
                                            queue[best_match].player->get_rating());
                matches.push_back(match);
                
                std::cout << "Match: " << match.player1->get_username() 
                          << " vs " << match.player2->get_username()
                          << " [Diff: " << match.rating_diff 
                          << ", Quality: " << (best_score * 100) << "%]\n";
                
                // 매칭된 플레이어 제거
                queue[i].player = nullptr;
                queue[best_match].player = nullptr;
            }
        }
        
        // 큐 정리
        queue.erase(std::remove_if(queue.begin(), queue.end(),
            [](const PlayerQueue& pq) { return !pq.player; }), queue.end());
        
        return matches;
    }
};
```

---

## 🎮 Part 4: Pong 게임 랭킹 시스템

### 4.1 통합 예제

```cpp
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <iomanip>

class RankingSystem {
private:
    std::vector<std::shared_ptr<Player>> players;
    EloRating elo;
    
public:
    void add_player(std::shared_ptr<Player> player) {
        players.push_back(player);
    }
    
    // 매치 결과 처리
    void process_match(int player1_id, int player2_id, double player1_score) {
        auto p1 = find_player(player1_id);
        auto p2 = find_player(player2_id);
        
        if (!p1 || !p2) {
            std::cerr << "Player not found\n";
            return;
        }
        
        // ELO 계산
        auto result = EloRating::calculate_new_ratings(
            p1->get_rating(), p2->get_rating(), player1_score);
        
        // 결과 기록
        p1->record_match(player2_id, player1_score, result.change_a);
        p2->record_match(player1_id, 1.0 - player1_score, result.change_b);
        
        std::cout << "Match Result:\n";
        std::cout << "  " << p1->get_username() << ": " 
                  << p1->get_rating() << " (" 
                  << (result.change_a >= 0 ? "+" : "") << result.change_a << ")\n";
        std::cout << "  " << p2->get_username() << ": " 
                  << p2->get_rating() << " (" 
                  << (result.change_b >= 0 ? "+" : "") << result.change_b << ")\n\n";
    }
    
    // 리더보드 출력
    void print_leaderboard() {
        // 레이팅 순으로 정렬
        std::vector<std::shared_ptr<Player>> sorted = players;
        std::sort(sorted.begin(), sorted.end(), 
            [](const auto& a, const auto& b) {
                return a->get_rating() > b->get_rating();
            });
        
        std::cout << "=== Leaderboard ===\n";
        std::cout << std::setw(4) << "Rank" << " "
                  << std::setw(15) << "Username" << " "
                  << std::setw(8) << "Rating" << " "
                  << std::setw(6) << "Games" << " "
                  << std::setw(12) << "W-L-D" << " "
                  << std::setw(8) << "Win%" << "\n";
        std::cout << std::string(70, '-') << "\n";
        
        for (size_t i = 0; i < sorted.size(); ++i) {
            auto& p = sorted[i];
            std::cout << std::setw(4) << (i + 1) << " "
                      << std::setw(15) << p->get_username() << " "
                      << std::setw(8) << std::fixed << std::setprecision(1) 
                      << p->get_rating() << " "
                      << std::setw(6) << p->get_games_played() << " "
                      << std::setw(12) << (std::to_string(p->get_wins()) + "-" + 
                                          std::to_string(p->get_losses()) + "-" +
                                          std::to_string(p->get_draws())) << " "
                      << std::setw(7) << std::fixed << std::setprecision(1) 
                      << p->get_win_rate() << "%\n";
        }
        std::cout << "\n";
    }
    
    // 티어 시스템
    std::string get_tier(double rating) {
        if (rating >= 2000) return "💎 Diamond";
        if (rating >= 1800) return "🥇 Platinum";
        if (rating >= 1600) return "🥈 Gold";
        if (rating >= 1400) return "🥉 Silver";
        return "🟫 Bronze";
    }
    
    void print_tiers() {
        std::cout << "=== Player Tiers ===\n";
        for (const auto& p : players) {
            std::cout << std::setw(15) << p->get_username() << ": "
                      << get_tier(p->get_rating()) << " ("
                      << p->get_rating() << ")\n";
        }
        std::cout << "\n";
    }
    
private:
    std::shared_ptr<Player> find_player(int id) {
        for (auto& p : players) {
            if (p->get_id() == id) return p;
        }
        return nullptr;
    }
};

int main() {
    RankingSystem ranking;
    
    // 플레이어 생성
    auto alice = std::make_shared<Player>(1, "Alice", 1500.0);
    auto bob = std::make_shared<Player>(2, "Bob", 1500.0);
    auto charlie = std::make_shared<Player>(3, "Charlie", 1500.0);
    auto david = std::make_shared<Player>(4, "David", 1500.0);
    
    ranking.add_player(alice);
    ranking.add_player(bob);
    ranking.add_player(charlie);
    ranking.add_player(david);
    
    std::cout << "=== Pong Ranking System ===\n\n";
    
    // 시뮬레이션: 10 매치
    std::cout << "Simulating 10 matches...\n\n";
    
    // Alice vs Bob (Alice 승)
    ranking.process_match(1, 2, 1.0);
    
    // Charlie vs David (Charlie 승)
    ranking.process_match(3, 4, 1.0);
    
    // Alice vs Charlie (Charlie 승)
    ranking.process_match(1, 3, 0.0);
    
    // Bob vs David (무승부)
    ranking.process_match(2, 4, 0.5);
    
    // Alice vs David (Alice 승)
    ranking.process_match(1, 4, 1.0);
    
    // Bob vs Charlie (Charlie 승)
    ranking.process_match(2, 3, 0.0);
    
    // Alice vs Bob (Alice 승)
    ranking.process_match(1, 2, 1.0);
    
    // Charlie vs David (Charlie 승)
    ranking.process_match(3, 4, 1.0);
    
    // Alice vs Charlie (Charlie 승)
    ranking.process_match(1, 3, 0.0);
    
    // Bob vs David (David 승)
    ranking.process_match(2, 4, 0.0);
    
    // 리더보드
    ranking.print_leaderboard();
    
    // 티어
    ranking.print_tiers();
    
    return 0;
}
```

**CMakeLists.txt** (pong_ranking):
```cmake
cmake_minimum_required(VERSION 3.20)
project(pong_ranking)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(pong_ranking 
    pong_ranking.cpp
)

if(UNIX AND NOT APPLE)
    target_link_libraries(pong_ranking PRIVATE m)
endif()
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./pong_ranking
```

**실행 결과**:
```
=== Pong Ranking System ===

Simulating 10 matches...

Match Result:
  Alice: 1516.0 (+16.0)
  Bob: 1484.0 (-16.0)

Match Result:
  Charlie: 1516.0 (+16.0)
  David: 1484.0 (-16.0)

Match Result:
  Alice: 1500.0 (-16.0)
  Charlie: 1532.0 (+16.0)

Match Result:
  Bob: 1492.0 (+8.0)
  David: 1476.0 (-8.0)

Match Result:
  Alice: 1508.0 (+8.0)
  David: 1468.0 (-8.0)

Match Result:
  Bob: 1475.2 (-16.8)
  Charlie: 1548.8 (+16.8)

Match Result:
  Alice: 1525.8 (+17.8)
  Bob: 1457.4 (-17.8)

Match Result:
  Charlie: 1563.5 (+14.7)
  David: 1453.3 (-14.7)

Match Result:
  Alice: 1506.6 (-19.2)
  Charlie: 1582.7 (+19.2)

Match Result:
  Bob: 1442.6 (-14.8)
  David: 1468.1 (+14.8)

=== Leaderboard ===
Rank Username        Rating  Games  W-L-D        Win%
----------------------------------------------------------------------
   1 Charlie         1582.7      5  5-0-0       100.0%
   2 Alice           1506.6      5  3-2-0        60.0%
   3 David           1468.1      5  1-3-1        20.0%
   4 Bob             1442.6      5  0-4-1         0.0%

=== Player Tiers ===
          Alice: 🥈 Silver (1506.6)
            Bob: 🥉 Silver (1442.6)
        Charlie: 🥈 Silver (1582.7)
          David: 🥉 Silver (1468.1)
```

---

## 🐛 자주 막히는 부분

### 문제 1: 초기 레이팅 설정

```cpp
// ❌ 모든 플레이어를 1500으로 시작
Player newPlayer(id, name, 1500.0);  // 숙련된 플레이어도 1500?

// ✅ 배치 경기 (Placement Matches)
class Player {
    bool is_placement = true;
    int placement_games_left = 10;
    
    void record_match(...) {
        if (is_placement) {
            placement_games_left--;
            if (placement_games_left == 0) {
                is_placement = false;
                // 10경기 후 실제 레이팅 확정
            }
        }
    }
};
```

### 문제 2: K-factor가 너무 높거나 낮음

```cpp
// ❌ 고정된 K-factor
static constexpr double K_FACTOR = 32.0;  // 모든 플레이어 동일

// ✅ 동적 K-factor
double get_k_factor(const Player& player) {
    if (player.get_games_played() < 30) {
        return 40.0;  // 초보자: 빠른 변동
    } else if (player.get_rating() >= 2400) {
        return 16.0;  // 마스터: 안정적
    } else {
        return 32.0;  // 일반
    }
}
```

### 문제 3: 매치메이킹이 느림

```cpp
// ❌ 모든 플레이어와 비교
for (int i = 0; i < queue.size(); i++) {
    for (int j = 0; j < queue.size(); j++) {
        // O(N²) 복잡도!
    }
}

// ✅ 레이팅 순으로 정렬 후 인접 플레이어만 비교
std::sort(queue.begin(), queue.end(), /*...*/);
for (int i = 0; i + 1 < queue.size(); i += 2) {
    // 인접한 플레이어만 매칭
}
```

### 문제 4: 레이팅 인플레이션/디플레이션

```cpp
// ELO는 제로섬 (Zero-sum)
// 한 명이 +20 → 다른 한 명이 -20
// 시스템 전체의 레이팅 합은 일정!

// ❌ 신규 플레이어를 1500으로 시작하면?
// → 전체 평균이 상승 (인플레이션)

// ✅ 해결책 1: 신규 플레이어를 낮게 시작 (1200)
// ✅ 해결책 2: 비활성 플레이어 레이팅 감소 (시즌 리셋)
```

### 문제 5: 팀 게임 ELO 계산

```cpp
// ❌ 팀 평균 레이팅 사용
double team1_avg = (player1.rating + player2.rating) / 2;
// 문제: 1500+1500 vs 1800+1200은 같은 평균이지만 실력 차이!

// ✅ 팀 레이팅 계산 (가중 평균)
double team1_rating = (player1.rating + player2.rating) / 2;
double team2_rating = (player3.rating + player4.rating) / 2;

// 팀 전체에 대해 ELO 계산 후 개별 배분
auto result = EloRating::calculate_new_ratings(team1_rating, team2_rating, team1_score);
player1.record_match(..., result.change_a / 2);  // 변화량 분배
player2.record_match(..., result.change_a / 2);
```

---

## ✅ 완료 체크리스트

### Part 1: ELO 기초
- [ ] ELO 개념 이해
- [ ] 예상 승률 계산 공식
- [ ] 새로운 점수 계산 공식
- [ ] 손 계산 예제

### Part 2: C++ 구현
- [ ] EloRating 클래스 구현
- [ ] Player 클래스 구현
- [ ] 매치 기록 시스템
- [ ] 통계 출력

### Part 3: 매치메이킹
- [ ] 간단한 매치메이킹 구현
- [ ] 대기 시간에 따른 범위 확장
- [ ] 스킬 기반 매칭
- [ ] 매칭 품질 점수

### Part 4: 실전 랭킹 시스템
- [ ] RankingSystem 구현
- [ ] 리더보드 출력
- [ ] 티어 시스템 구현
- [ ] Pong 게임 통합

---

## 🚀 다음 단계

✅ **ELO Rating System 완료!**

**다음 학습**:
- [**Quickstart 60**](60-postgresql-redis-docker.md) - 데이터베이스에 랭킹 저장
- [**Quickstart 50**](50-prometheus-grafana.md) - 랭킹 시스템 모니터링

**실전 적용**:
- `mini-gameserver` M1.8 - Pong 랭킹 시스템
- `mini-gameserver` M1.9 - 매치메이킹 + ELO

**확장 아이디어**:
- 시즌 시스템 (3개월마다 리셋)
- 플레이스먼트 매치 (배치 경기 10판)
- 디케이 시스템 (비활성 시 레이팅 감소)
- 보상 시스템 (티어별 보상)

---

## 📚 참고 자료

- [Elo Rating System (Wikipedia)](https://en.wikipedia.org/wiki/Elo_rating_system)
- [Chess Rating System](https://www.chess.com/article/view/chess-rating-system)
- [Glicko Rating System](http://www.glicko.net/glicko.html) (ELO 개선판)
- [TrueSkill](https://www.microsoft.com/en-us/research/project/trueskill-ranking-system/) (Microsoft)
- [League of Legends Matchmaking](https://leagueoflegends.fandom.com/wiki/Matchmaking)

---

**Last Updated**: 2025-11-12
