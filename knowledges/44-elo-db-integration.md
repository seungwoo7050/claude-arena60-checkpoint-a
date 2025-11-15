# Quickstart 44: ELO Rating + PostgreSQL Integration

**목표**: ELO 랭킹 시스템을 PostgreSQL 데이터베이스와 연동하여 게임 서버에 통합합니다.

**대상**: `mini-gameserver` Phase 1 Milestone 1.8, 1.11 (ELO + DB 통합)

**난이도**: ⭐⭐⭐⭐⭐ (Advanced)

**소요 시간**: 90분

**선행 학습**:
- 52-elo-rating-system.md (ELO 알고리즘)
- 60-postgresql-redis-docker.md (PostgreSQL 기초)
- 43-jwt-game-integration.md (JWT 인증)

**학습 목표**:
1. PostgreSQL 스키마 설계 (사용자, 매치 기록)
2. ELO 계산기를 C++로 구현
3. 매치 결과를 데이터베이스에 저장
4. 리더보드 API 구현
5. Redis 캐싱으로 성능 최적화

---

## Part 1: PostgreSQL 스키마 설계 (15분)

### 1.1 데이터베이스 스키마

```sql
-- migrations/002_add_elo.sql
-- ELO 랭킹 시스템 스키마

-- 사용자 테이블 (기존 테이블 확장)
ALTER TABLE users ADD COLUMN IF NOT EXISTS elo_rating INTEGER DEFAULT 1000;
ALTER TABLE users ADD COLUMN IF NOT EXISTS games_played INTEGER DEFAULT 0;
ALTER TABLE users ADD COLUMN IF NOT EXISTS wins INTEGER DEFAULT 0;
ALTER TABLE users ADD COLUMN IF NOT EXISTS losses INTEGER DEFAULT 0;
ALTER TABLE users ADD COLUMN IF NOT EXISTS win_rate FLOAT DEFAULT 0.0;

-- 매치 기록 테이블
CREATE TABLE IF NOT EXISTS matches (
    id SERIAL PRIMARY KEY,
    match_id VARCHAR(64) UNIQUE NOT NULL,
    room_id INTEGER NOT NULL,
    started_at TIMESTAMP NOT NULL DEFAULT NOW(),
    ended_at TIMESTAMP,
    duration_seconds INTEGER,
    game_mode VARCHAR(32) DEFAULT 'pong',
    status VARCHAR(16) DEFAULT 'in_progress',  -- in_progress, completed, abandoned
    created_at TIMESTAMP NOT NULL DEFAULT NOW()
);

-- 매치 참가자 테이블
CREATE TABLE IF NOT EXISTS match_participants (
    id SERIAL PRIMARY KEY,
    match_id VARCHAR(64) REFERENCES matches(match_id) ON DELETE CASCADE,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    team INTEGER DEFAULT 0,  -- 0 = left, 1 = right (Pong의 경우)
    score INTEGER DEFAULT 0,
    elo_before INTEGER NOT NULL,
    elo_after INTEGER NOT NULL,
    elo_change INTEGER NOT NULL,
    result VARCHAR(16),  -- win, loss, draw
    created_at TIMESTAMP NOT NULL DEFAULT NOW(),
    UNIQUE(match_id, user_id)
);

-- 리더보드 뷰 (빠른 조회를 위한 materialized view)
CREATE MATERIALIZED VIEW IF NOT EXISTS leaderboard AS
SELECT 
    u.id,
    u.username,
    u.email,
    u.elo_rating,
    u.games_played,
    u.wins,
    u.losses,
    u.win_rate,
    ROW_NUMBER() OVER (ORDER BY u.elo_rating DESC) as rank
FROM users u
WHERE u.games_played >= 5  -- 최소 5게임 이상 플레이한 유저만
ORDER BY u.elo_rating DESC;

-- 리더보드 새로고침 함수
CREATE OR REPLACE FUNCTION refresh_leaderboard()
RETURNS void AS $$
BEGIN
    REFRESH MATERIALIZED VIEW leaderboard;
END;
$$ LANGUAGE plpgsql;

-- 인덱스
CREATE INDEX IF NOT EXISTS idx_users_elo_rating ON users(elo_rating DESC);
CREATE INDEX IF NOT EXISTS idx_matches_status ON matches(status);
CREATE INDEX IF NOT EXISTS idx_matches_started_at ON matches(started_at DESC);
CREATE INDEX IF NOT EXISTS idx_match_participants_user_id ON match_participants(user_id);
CREATE INDEX IF NOT EXISTS idx_match_participants_match_id ON match_participants(match_id);

-- 통계 뷰
CREATE OR REPLACE VIEW user_stats AS
SELECT 
    u.id,
    u.username,
    u.elo_rating,
    u.games_played,
    u.wins,
    u.losses,
    u.win_rate,
    COUNT(DISTINCT mp.match_id) as total_matches,
    AVG(mp.elo_change) as avg_elo_change,
    MAX(mp.elo_after) as peak_elo,
    MIN(mp.elo_after) as lowest_elo
FROM users u
LEFT JOIN match_participants mp ON u.id = mp.user_id
GROUP BY u.id, u.username, u.elo_rating, u.games_played, u.wins, u.losses, u.win_rate;

-- 최근 매치 기록 조회 함수
CREATE OR REPLACE FUNCTION get_recent_matches(user_id_param INTEGER, limit_param INTEGER DEFAULT 10)
RETURNS TABLE (
    match_id VARCHAR,
    started_at TIMESTAMP,
    duration_seconds INTEGER,
    opponent_username VARCHAR,
    user_score INTEGER,
    opponent_score INTEGER,
    result VARCHAR,
    elo_change INTEGER,
    elo_after INTEGER
) AS $$
BEGIN
    RETURN QUERY
    SELECT 
        m.match_id,
        m.started_at,
        m.duration_seconds,
        u_opp.username as opponent_username,
        mp_user.score as user_score,
        mp_opp.score as opponent_score,
        mp_user.result,
        mp_user.elo_change,
        mp_user.elo_after
    FROM match_participants mp_user
    JOIN matches m ON mp_user.match_id = m.match_id
    JOIN match_participants mp_opp ON m.match_id = mp_opp.match_id 
        AND mp_opp.user_id != user_id_param
    JOIN users u_opp ON mp_opp.user_id = u_opp.id
    WHERE mp_user.user_id = user_id_param
        AND m.status = 'completed'
    ORDER BY m.started_at DESC
    LIMIT limit_param;
END;
$$ LANGUAGE plpgsql;
```

### 1.2 샘플 데이터 삽입

```sql
-- 테스트 사용자 생성
INSERT INTO users (username, email, password_hash, elo_rating) VALUES
    ('alice', 'alice@example.com', '$2a$10$...', 1200),
    ('bob', 'bob@example.com', '$2a$10$...', 1150),
    ('charlie', 'charlie@example.com', '$2a$10$...', 1300),
    ('diana', 'diana@example.com', '$2a$10$...', 1050)
ON CONFLICT (username) DO NOTHING;

-- 샘플 매치 생성
INSERT INTO matches (match_id, room_id, started_at, ended_at, duration_seconds, status) VALUES
    ('match_001', 1, NOW() - INTERVAL '1 hour', NOW() - INTERVAL '55 minutes', 300, 'completed'),
    ('match_002', 2, NOW() - INTERVAL '30 minutes', NOW() - INTERVAL '25 minutes', 300, 'completed');

-- 매치 참가자 기록
INSERT INTO match_participants (match_id, user_id, team, score, elo_before, elo_after, elo_change, result) VALUES
    ('match_001', 1, 0, 5, 1200, 1215, 15, 'win'),
    ('match_001', 2, 1, 3, 1150, 1135, -15, 'loss'),
    ('match_002', 3, 0, 5, 1300, 1308, 8, 'win'),
    ('match_002', 4, 1, 2, 1050, 1042, -8, 'loss');

-- 리더보드 갱신
SELECT refresh_leaderboard();
```

---

## Part 2: ELO 계산기 C++ 구현 (20분)

### 2.1 ELO Calculator 클래스

```cpp
// elo_calculator.h
#pragma once
#include <cmath>
#include <algorithm>

class EloCalculator {
private:
    int k_factor_;  // K-factor (16, 24, 32)
    
public:
    explicit EloCalculator(int k_factor = 32) : k_factor_(k_factor) {}
    
    // 예상 승률 계산
    double calculate_expected_score(int rating_a, int rating_b) const {
        return 1.0 / (1.0 + std::pow(10.0, (rating_b - rating_a) / 400.0));
    }
    
    // ELO 변화량 계산
    struct RatingChange {
        int winner_new_rating;
        int loser_new_rating;
        int winner_change;
        int loser_change;
    };
    
    RatingChange calculate_rating_change(
        int winner_rating,
        int loser_rating
    ) const {
        // 예상 승률
        double expected_winner = calculate_expected_score(winner_rating, loser_rating);
        double expected_loser = calculate_expected_score(loser_rating, winner_rating);
        
        // 실제 결과 (승자 = 1, 패자 = 0)
        double actual_winner = 1.0;
        double actual_loser = 0.0;
        
        // ELO 변화량
        int winner_change = static_cast<int>(
            std::round(k_factor_ * (actual_winner - expected_winner))
        );
        int loser_change = static_cast<int>(
            std::round(k_factor_ * (actual_loser - expected_loser))
        );
        
        RatingChange result;
        result.winner_new_rating = winner_rating + winner_change;
        result.loser_new_rating = loser_rating + loser_change;
        result.winner_change = winner_change;
        result.loser_change = loser_change;
        
        return result;
    }
    
    // 무승부 처리
    struct DrawRatingChange {
        int player_a_new_rating;
        int player_b_new_rating;
        int player_a_change;
        int player_b_change;
    };
    
    DrawRatingChange calculate_draw_rating_change(
        int rating_a,
        int rating_b
    ) const {
        double expected_a = calculate_expected_score(rating_a, rating_b);
        double expected_b = calculate_expected_score(rating_b, rating_a);
        
        // 무승부는 0.5점
        double actual = 0.5;
        
        int change_a = static_cast<int>(
            std::round(k_factor_ * (actual - expected_a))
        );
        int change_b = static_cast<int>(
            std::round(k_factor_ * (actual - expected_b))
        );
        
        DrawRatingChange result;
        result.player_a_new_rating = rating_a + change_a;
        result.player_b_new_rating = rating_b + change_b;
        result.player_a_change = change_a;
        result.player_b_change = change_b;
        
        return result;
    }
    
    // K-factor 동적 조정 (선택적)
    int get_k_factor(int rating, int games_played) const {
        // FIDE 체스 규칙 기반
        if (games_played < 30) {
            return 40;  // 신규 플레이어: 빠른 등급 변화
        } else if (rating < 2400) {
            return 20;  // 일반 플레이어
        } else {
            return 10;  // 고수: 안정적인 등급
        }
    }
};
```

### 2.2 단위 테스트

```cpp
// elo_calculator_test.cpp
#include "elo_calculator.h"
#include <cassert>
#include <iostream>
#include <cmath>

void test_expected_score() {
    EloCalculator calc;
    
    // 동일 레이팅: 50% 승률
    double expected = calc.calculate_expected_score(1200, 1200);
    assert(std::abs(expected - 0.5) < 0.01);
    
    // 400점 차이: 약 91% 승률
    expected = calc.calculate_expected_score(1600, 1200);
    assert(expected > 0.90 && expected < 0.92);
    
    std::cout << "✅ test_expected_score passed\n";
}

void test_rating_change() {
    EloCalculator calc(32);
    
    // 1200 vs 1200 (동일 레이팅)
    auto result = calc.calculate_rating_change(1200, 1200);
    assert(result.winner_change == 16);  // K/2
    assert(result.loser_change == -16);
    assert(result.winner_new_rating == 1216);
    assert(result.loser_new_rating == 1184);
    
    std::cout << "✅ test_rating_change passed\n";
}

void test_upset_victory() {
    EloCalculator calc(32);
    
    // 약자가 강자를 이김 (1000 vs 1400)
    auto result = calc.calculate_rating_change(1000, 1400);
    
    // 약자는 많이 올라가야 함
    assert(result.winner_change > 25);
    // 강자는 많이 떨어져야 함
    assert(result.loser_change < -25);
    
    std::cout << "✅ test_upset_victory passed (winner: +" 
              << result.winner_change << ", loser: " 
              << result.loser_change << ")\n";
}

void test_draw() {
    EloCalculator calc(32);
    
    auto result = calc.calculate_draw_rating_change(1200, 1200);
    
    // 동일 레이팅 무승부: 변화 없음
    assert(result.player_a_change == 0);
    assert(result.player_b_change == 0);
    
    std::cout << "✅ test_draw passed\n";
}

void test_dynamic_k_factor() {
    EloCalculator calc;
    
    // 신규 플레이어
    int k = calc.get_k_factor(1000, 10);
    assert(k == 40);
    
    // 일반 플레이어
    k = calc.get_k_factor(1500, 50);
    assert(k == 20);
    
    // 고수
    k = calc.get_k_factor(2500, 100);
    assert(k == 10);
    
    std::cout << "✅ test_dynamic_k_factor passed\n";
}

int main() {
    test_expected_score();
    test_rating_change();
    test_upset_victory();
    test_draw();
    test_dynamic_k_factor();
    
    std::cout << "\n🎉 All ELO calculator tests passed!\n";
    return 0;
}
```

---

## Part 3: PostgreSQL 통합 (30분)

### 3.1 Database Client 클래스

```cpp
// database_client.h
#pragma once
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <optional>
#include <memory>

struct UserProfile {
    int id;
    std::string username;
    std::string email;
    int elo_rating;
    int games_played;
    int wins;
    int losses;
    double win_rate;
};

struct MatchRecord {
    std::string match_id;
    std::string opponent_username;
    int user_score;
    int opponent_score;
    std::string result;  // "win", "loss", "draw"
    int elo_change;
    int elo_after;
    std::string started_at;
};

struct LeaderboardEntry {
    int rank;
    std::string username;
    int elo_rating;
    int games_played;
    int wins;
    int losses;
    double win_rate;
};

class DatabaseClient {
private:
    std::string connection_string_;
    std::unique_ptr<pqxx::connection> conn_;

public:
    explicit DatabaseClient(const std::string& conn_str)
        : connection_string_(conn_str)
    {
        connect();
    }

    void connect() {
        try {
            conn_ = std::make_unique<pqxx::connection>(connection_string_);
            if (!conn_->is_open()) {
                throw std::runtime_error("Failed to open database connection");
            }
            std::cout << "✅ Connected to PostgreSQL: " 
                      << conn_->dbname() << "\n";
        } catch (const std::exception& e) {
            std::cerr << "❌ Database connection error: " << e.what() << "\n";
            throw;
        }
    }

    // 사용자 프로필 조회
    std::optional<UserProfile> get_user_profile(int user_id) {
        try {
            pqxx::work txn(*conn_);
            
            auto result = txn.exec_params(
                "SELECT id, username, email, elo_rating, games_played, "
                "       wins, losses, win_rate "
                "FROM users WHERE id = $1",
                user_id
            );
            
            if (result.empty()) {
                return std::nullopt;
            }
            
            auto row = result[0];
            UserProfile profile;
            profile.id = row["id"].as<int>();
            profile.username = row["username"].as<std::string>();
            profile.email = row["email"].as<std::string>();
            profile.elo_rating = row["elo_rating"].as<int>();
            profile.games_played = row["games_played"].as<int>();
            profile.wins = row["wins"].as<int>();
            profile.losses = row["losses"].as<int>();
            profile.win_rate = row["win_rate"].as<double>();
            
            return profile;
            
        } catch (const std::exception& e) {
            std::cerr << "Error getting user profile: " << e.what() << "\n";
            return std::nullopt;
        }
    }

    // 매치 시작 기록
    bool create_match(const std::string& match_id, int room_id) {
        try {
            pqxx::work txn(*conn_);
            
            txn.exec_params(
                "INSERT INTO matches (match_id, room_id, status) "
                "VALUES ($1, $2, 'in_progress')",
                match_id, room_id
            );
            
            txn.commit();
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Error creating match: " << e.what() << "\n";
            return false;
        }
    }

    // 매치 완료 및 ELO 업데이트
    bool complete_match(
        const std::string& match_id,
        int winner_id,
        int loser_id,
        int winner_score,
        int loser_score,
        int winner_elo_before,
        int loser_elo_before,
        int winner_elo_after,
        int loser_elo_after,
        int duration_seconds
    ) {
        try {
            pqxx::work txn(*conn_);
            
            // 매치 상태 업데이트
            txn.exec_params(
                "UPDATE matches SET "
                "ended_at = NOW(), "
                "duration_seconds = $2, "
                "status = 'completed' "
                "WHERE match_id = $1",
                match_id, duration_seconds
            );
            
            // 승자 참가 기록
            txn.exec_params(
                "INSERT INTO match_participants "
                "(match_id, user_id, team, score, elo_before, elo_after, "
                " elo_change, result) "
                "VALUES ($1, $2, 0, $3, $4, $5, $6, 'win')",
                match_id, winner_id, winner_score,
                winner_elo_before, winner_elo_after,
                winner_elo_after - winner_elo_before
            );
            
            // 패자 참가 기록
            txn.exec_params(
                "INSERT INTO match_participants "
                "(match_id, user_id, team, score, elo_before, elo_after, "
                " elo_change, result) "
                "VALUES ($1, $2, 1, $3, $4, $5, $6, 'loss')",
                match_id, loser_id, loser_score,
                loser_elo_before, loser_elo_after,
                loser_elo_after - loser_elo_before
            );
            
            // 승자 통계 업데이트
            txn.exec_params(
                "UPDATE users SET "
                "elo_rating = $2, "
                "games_played = games_played + 1, "
                "wins = wins + 1, "
                "win_rate = CAST(wins + 1 AS FLOAT) / (games_played + 1) "
                "WHERE id = $1",
                winner_id, winner_elo_after
            );
            
            // 패자 통계 업데이트
            txn.exec_params(
                "UPDATE users SET "
                "elo_rating = $2, "
                "games_played = games_played + 1, "
                "losses = losses + 1, "
                "win_rate = CAST(wins AS FLOAT) / (games_played + 1) "
                "WHERE id = $1",
                loser_id, loser_elo_after
            );
            
            txn.commit();
            
            std::cout << "✅ Match completed: " << match_id 
                      << " (Winner: " << winner_id << " +" 
                      << (winner_elo_after - winner_elo_before) << ")\n";
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Error completing match: " << e.what() << "\n";
            return false;
        }
    }

    // 최근 매치 기록 조회
    std::vector<MatchRecord> get_recent_matches(int user_id, int limit = 10) {
        std::vector<MatchRecord> matches;
        
        try {
            pqxx::work txn(*conn_);
            
            auto result = txn.exec_params(
                "SELECT * FROM get_recent_matches($1, $2)",
                user_id, limit
            );
            
            for (const auto& row : result) {
                MatchRecord match;
                match.match_id = row["match_id"].as<std::string>();
                match.opponent_username = row["opponent_username"].as<std::string>();
                match.user_score = row["user_score"].as<int>();
                match.opponent_score = row["opponent_score"].as<int>();
                match.result = row["result"].as<std::string>();
                match.elo_change = row["elo_change"].as<int>();
                match.elo_after = row["elo_after"].as<int>();
                match.started_at = row["started_at"].as<std::string>();
                
                matches.push_back(match);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error getting recent matches: " << e.what() << "\n";
        }
        
        return matches;
    }

    // 리더보드 조회
    std::vector<LeaderboardEntry> get_leaderboard(int limit = 100) {
        std::vector<LeaderboardEntry> leaderboard;
        
        try {
            pqxx::work txn(*conn_);
            
            // Materialized View 새로고침
            txn.exec("SELECT refresh_leaderboard()");
            
            auto result = txn.exec_params(
                "SELECT rank, username, elo_rating, games_played, "
                "       wins, losses, win_rate "
                "FROM leaderboard "
                "LIMIT $1",
                limit
            );
            
            for (const auto& row : result) {
                LeaderboardEntry entry;
                entry.rank = row["rank"].as<int>();
                entry.username = row["username"].as<std::string>();
                entry.elo_rating = row["elo_rating"].as<int>();
                entry.games_played = row["games_played"].as<int>();
                entry.wins = row["wins"].as<int>();
                entry.losses = row["losses"].as<int>();
                entry.win_rate = row["win_rate"].as<double>();
                
                leaderboard.push_back(entry);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error getting leaderboard: " << e.what() << "\n";
        }
        
        return leaderboard;
    }

    // 사용자 랭킹 조회
    int get_user_rank(int user_id) {
        try {
            pqxx::work txn(*conn_);
            
            auto result = txn.exec_params(
                "SELECT rank FROM leaderboard WHERE id = $1",
                user_id
            );
            
            if (result.empty()) {
                return -1;
            }
            
            return result[0]["rank"].as<int>();
            
        } catch (const std::exception& e) {
            std::cerr << "Error getting user rank: " << e.what() << "\n";
            return -1;
        }
    }
};
```

---

## Part 4: 게임 서버 통합 (20분)

### 4.1 Match Manager 클래스

```cpp
// match_manager.h
#pragma once
#include "elo_calculator.h"
#include "database_client.h"
#include <memory>
#include <string>
#include <random>

class MatchManager {
private:
    std::shared_ptr<EloCalculator> elo_calc_;
    std::shared_ptr<DatabaseClient> db_;
    std::mt19937 rng_;

public:
    MatchManager(
        std::shared_ptr<EloCalculator> elo_calc,
        std::shared_ptr<DatabaseClient> db
    ) : elo_calc_(elo_calc), db_(db), rng_(std::random_device{}())
    {
    }

    // 매치 ID 생성
    std::string generate_match_id() {
        std::uniform_int_distribution<uint64_t> dist;
        uint64_t id = dist(rng_);
        return "match_" + std::to_string(id);
    }

    // 매치 시작
    std::string start_match(int room_id) {
        std::string match_id = generate_match_id();
        
        if (db_->create_match(match_id, room_id)) {
            std::cout << "🎮 Match started: " << match_id << "\n";
            return match_id;
        }
        
        return "";
    }

    // 매치 종료 및 ELO 업데이트
    bool end_match(
        const std::string& match_id,
        int winner_id,
        int loser_id,
        int winner_score,
        int loser_score,
        int duration_seconds
    ) {
        // 현재 ELO 조회
        auto winner_profile = db_->get_user_profile(winner_id);
        auto loser_profile = db_->get_user_profile(loser_id);
        
        if (!winner_profile || !loser_profile) {
            std::cerr << "Failed to get user profiles\n";
            return false;
        }
        
        int winner_elo_before = winner_profile->elo_rating;
        int loser_elo_before = loser_profile->elo_rating;
        
        // ELO 변화 계산
        auto rating_change = elo_calc_->calculate_rating_change(
            winner_elo_before,
            loser_elo_before
        );
        
        // 데이터베이스 업데이트
        bool success = db_->complete_match(
            match_id,
            winner_id,
            loser_id,
            winner_score,
            loser_score,
            winner_elo_before,
            loser_elo_before,
            rating_change.winner_new_rating,
            rating_change.loser_new_rating,
            duration_seconds
        );
        
        if (success) {
            std::cout << "📊 ELO Updated:\n";
            std::cout << "  Winner: " << winner_elo_before 
                      << " → " << rating_change.winner_new_rating 
                      << " (+" << rating_change.winner_change << ")\n";
            std::cout << "  Loser: " << loser_elo_before 
                      << " → " << rating_change.loser_new_rating 
                      << " (" << rating_change.loser_change << ")\n";
        }
        
        return success;
    }

    // 사용자 통계 조회
    void print_user_stats(int user_id) {
        auto profile = db_->get_user_profile(user_id);
        if (!profile) {
            std::cout << "User not found\n";
            return;
        }
        
        int rank = db_->get_user_rank(user_id);
        
        std::cout << "\n=== User Stats ===\n";
        std::cout << "Username: " << profile->username << "\n";
        std::cout << "Rank: #" << rank << "\n";
        std::cout << "ELO Rating: " << profile->elo_rating << "\n";
        std::cout << "Games Played: " << profile->games_played << "\n";
        std::cout << "Record: " << profile->wins << "W - " 
                  << profile->losses << "L\n";
        std::cout << "Win Rate: " << (profile->win_rate * 100) << "%\n";
        
        // 최근 매치 기록
        auto matches = db_->get_recent_matches(user_id, 5);
        std::cout << "\nRecent Matches:\n";
        for (const auto& match : matches) {
            std::cout << "  vs " << match.opponent_username 
                      << ": " << match.user_score << "-" << match.opponent_score
                      << " (" << match.result << ") "
                      << (match.elo_change >= 0 ? "+" : "") << match.elo_change
                      << "\n";
        }
    }

    // 리더보드 출력
    void print_leaderboard(int limit = 10) {
        auto leaderboard = db_->get_leaderboard(limit);
        
        std::cout << "\n=== Leaderboard (Top " << limit << ") ===\n";
        std::cout << "Rank | Username        | ELO  | Games | W-L  | Win%\n";
        std::cout << "-----+-----------------+------+-------+------+------\n";
        
        for (const auto& entry : leaderboard) {
            printf("%-4d | %-15s | %4d | %5d | %2d-%2d | %4.1f%%\n",
                entry.rank,
                entry.username.c_str(),
                entry.elo_rating,
                entry.games_played,
                entry.wins,
                entry.losses,
                entry.win_rate * 100
            );
        }
    }
};
```

### 4.2 사용 예제

```cpp
// match_example.cpp
#include "match_manager.h"
#include <iostream>

int main() {
    // 초기화
    auto elo_calc = std::make_shared<EloCalculator>(32);
    auto db = std::make_shared<DatabaseClient>(
        "postgresql://user:password@localhost/gamedb"
    );
    
    MatchManager manager(elo_calc, db);

    // 매치 시작
    std::string match_id = manager.start_match(1);
    
    // 게임 진행 (시뮬레이션)
    std::cout << "🎮 Game in progress...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 매치 종료 (alice wins vs bob, 5-3)
    manager.end_match(match_id, 1, 2, 5, 3, 300);
    
    // 통계 출력
    manager.print_user_stats(1);  // alice
    manager.print_user_stats(2);  // bob
    
    // 리더보드
    manager.print_leaderboard(10);
    
    return 0;
}
```

---

## Part 5: Redis 캐싱 (선택적, 10분)

### 5.1 Redis 캐시 래퍼

```cpp
// redis_cache.h
#pragma once
#include <hiredis/hiredis.h>
#include <string>
#include <optional>
#include <memory>

class RedisCache {
private:
    std::unique_ptr<redisContext, decltype(&redisFree)> context_;
    
public:
    RedisCache(const std::string& host = "127.0.0.1", int port = 6379)
        : context_(nullptr, redisFree)
    {
        context_.reset(redisConnect(host.c_str(), port));
        if (context_ == nullptr || context_->err) {
            throw std::runtime_error("Redis connection error");
        }
    }

    // 리더보드 캐싱 (TTL 60초)
    bool cache_leaderboard(const std::string& data) {
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(), "SETEX leaderboard 60 %s", data.c_str())
        );
        
        if (reply) {
            bool success = (reply->type == REDIS_REPLY_STATUS);
            freeReplyObject(reply);
            return success;
        }
        
        return false;
    }

    std::optional<std::string> get_leaderboard() {
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(), "GET leaderboard")
        );
        
        if (reply && reply->type == REDIS_REPLY_STRING) {
            std::string data(reply->str);
            freeReplyObject(reply);
            return data;
        }
        
        if (reply) freeReplyObject(reply);
        return std::nullopt;
    }

    // 사용자 프로필 캐싱
    bool cache_user_profile(int user_id, const std::string& data) {
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(), 
                "SETEX user:%d 300 %s", user_id, data.c_str())
        );
        
        if (reply) {
            bool success = (reply->type == REDIS_REPLY_STATUS);
            freeReplyObject(reply);
            return success;
        }
        
        return false;
    }

    std::optional<std::string> get_user_profile(int user_id) {
        auto reply = static_cast<redisReply*>(
            redisCommand(context_.get(), "GET user:%d", user_id)
        );
        
        if (reply && reply->type == REDIS_REPLY_STRING) {
            std::string data(reply->str);
            freeReplyObject(reply);
            return data;
        }
        
        if (reply) freeReplyObject(reply);
        return std::nullopt;
    }
};
```

---

## Troubleshooting

### 문제 1: "libpqxx/pqxx: No such file or directory"

**증상**:
```
fatal error: pqxx/pqxx: No such file or directory
```

**원인**: libpqxx 라이브러리 미설치

**해결**:
```bash
# Ubuntu/Debian
sudo apt-get install libpqxx-dev

# macOS
brew install libpqxx

# CMakeLists.txt에 추가
find_package(PostgreSQL REQUIRED)
target_link_libraries(your_target PRIVATE pqxx PostgreSQL::PostgreSQL)
```

---

### 문제 2: "materialized view does not exist"

**증상**:
```
ERROR: materialized view "leaderboard" does not exist
```

**원인**: 마이그레이션 미실행

**해결**:
```bash
psql -U gameuser -d gamedb -f migrations/002_add_elo.sql
```

---

### 문제 3: ELO 변화량이 너무 큼/작음

**증상**:
초보자가 고수를 이겼는데 ELO가 5점밖에 안 오름

**원인**:
K-factor가 너무 작음

**해결**:
```cpp
// K-factor 조정
EloCalculator calc(32);  // 기본값

// 또는 동적 K-factor 사용
int k = calc.get_k_factor(rating, games_played);
```

---

### 문제 4: 리더보드 조회가 느림

**증상**:
```
SELECT * FROM leaderboard  -- 5초 소요
```

**원인**:
Materialized View 새로고침 누락

**해결**:
```sql
-- 매치 종료마다 새로고침 (비추천)
SELECT refresh_leaderboard();

-- 또는 주기적 새로고침 (권장)
-- Cron job or 백그라운드 스레드
```

```cpp
// C++ 백그라운드 스레드
std::thread refresh_thread([&db]() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::minutes(5));
        
        pqxx::work txn(*db.get_connection());
        txn.exec("SELECT refresh_leaderboard()");
        txn.commit();
    }
});
```

---

### 문제 5: "unique constraint violation" 에러

**증상**:
```
ERROR: duplicate key value violates unique constraint "match_participants_match_id_user_id_key"
```

**원인**:
같은 유저가 같은 매치에 두 번 기록됨

**해결**:
```cpp
// 매치 종료 전 중복 체크
bool is_match_completed(const std::string& match_id) {
    pqxx::work txn(*conn_);
    auto result = txn.exec_params(
        "SELECT status FROM matches WHERE match_id = $1",
        match_id
    );
    
    if (result.empty()) return false;
    
    std::string status = result[0]["status"].as<std::string>();
    return (status == "completed");
}

// 매치 종료 시 체크
if (is_match_completed(match_id)) {
    std::cerr << "Match already completed\n";
    return false;
}
```

---

## 요약

이번 Quickstart에서 학습한 내용:

1. **PostgreSQL 스키마**: 사용자, 매치, 참가자 테이블 설계
2. **ELO Calculator**: C++로 ELO 알고리즘 구현
3. **Database Client**: libpqxx를 사용한 DB 연동
4. **Match Manager**: 매치 관리 및 ELO 업데이트 통합
5. **Redis 캐싱**: 리더보드 성능 최적화 (선택적)

**mini-gameserver Milestone 1.8, 1.11 완료!** ✅

**다음 단계**:
- 45-matchmaking-system.md: 매치메이킹 큐
- 46-grafana-dashboard.md: Grafana 대시보드

**주요 개념**:
- ELO 시스템은 상대적 실력을 측정
- K-factor는 등급 변화 속도 조절
- Materialized View로 리더보드 조회 최적화
- Redis 캐싱으로 API 응답 속도 향상
- 매치 기록으로 플레이어 성장 추적

이제 완전한 ELO 랭킹 시스템이 통합된 게임 서버를 만들 수 있습니다! 🏆🎮
