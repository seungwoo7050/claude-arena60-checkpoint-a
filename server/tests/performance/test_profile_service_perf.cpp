#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "arena60/stats/leaderboard_store.h"
#include "arena60/stats/player_profile_service.h"

TEST(PlayerProfileServicePerformanceTest, RecordsHundredMatchesUnderBudget) {
    auto leaderboard = std::make_shared<arena60::InMemoryLeaderboardStore>();
    arena60::PlayerProfileService service(leaderboard);

    const auto now = std::chrono::system_clock::now();
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        const std::string match_id = "match-" + std::to_string(i);
        std::vector<arena60::PlayerMatchStats> stats{
            arena60::PlayerMatchStats{match_id, "winner", 5, 5, 1, 0, 100, 10},
            arena60::PlayerMatchStats{match_id, "loser", 4, 2, 0, 1, 40, 100},
        };
        arena60::MatchResult result{match_id, "winner", "loser", now + std::chrono::seconds(i),
                                    stats};
        service.RecordMatch(result);
    }
    const auto finish = std::chrono::steady_clock::now();
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
    EXPECT_LE(elapsed_ms, 5);
}
