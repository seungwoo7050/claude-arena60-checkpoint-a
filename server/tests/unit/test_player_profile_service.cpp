#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "arena60/stats/leaderboard_store.h"
#include "arena60/stats/player_profile_service.h"

namespace {
using arena60::MatchResult;
using arena60::PlayerMatchStats;
}

TEST(PlayerProfileServiceTest, UpdatesRatingsAggregatesStatsAndSerializes) {
    auto leaderboard = std::make_shared<arena60::InMemoryLeaderboardStore>();
    arena60::PlayerProfileService service(leaderboard);

    const auto now = std::chrono::system_clock::now();

    std::vector<PlayerMatchStats> match1_stats{
        PlayerMatchStats{"match-1", "attacker", 5, 5, 1, 0, 100, 20},
        PlayerMatchStats{"match-1", "defender", 4, 2, 0, 1, 40, 100},
    };
    MatchResult match1{"match-1", "attacker", "defender", now, match1_stats};
    service.RecordMatch(match1);

    auto attacker_profile = service.GetProfile("attacker");
    ASSERT_TRUE(attacker_profile.has_value());
    EXPECT_EQ(1u, attacker_profile->matches);
    EXPECT_EQ(1u, attacker_profile->wins);
    EXPECT_EQ(0u, attacker_profile->losses);
    EXPECT_EQ(1213, attacker_profile->rating);
    EXPECT_EQ(100u, attacker_profile->damage_dealt);
    EXPECT_EQ(20u, attacker_profile->damage_taken);
    EXPECT_DOUBLE_EQ(1.0, attacker_profile->Accuracy());

    auto defender_profile = service.GetProfile("defender");
    ASSERT_TRUE(defender_profile.has_value());
    EXPECT_EQ(1188, defender_profile->rating);
    EXPECT_EQ(1u, defender_profile->losses);
    EXPECT_EQ(40u, defender_profile->damage_dealt);

    const std::string profile_json = service.SerializeProfile(*attacker_profile);
    EXPECT_NE(profile_json.find("\"player_id\":\"attacker\""), std::string::npos);
    EXPECT_NE(profile_json.find("\"accuracy\":1.0000"), std::string::npos);

    const std::string metrics = service.MetricsSnapshot();
    EXPECT_NE(metrics.find("player_profiles_total 2"), std::string::npos);
    EXPECT_NE(metrics.find("matches_recorded_total 1"), std::string::npos);
    EXPECT_NE(metrics.find("rating_updates_total 2"), std::string::npos);

    std::vector<PlayerMatchStats> match2_stats{
        PlayerMatchStats{"match-2", "defender", 6, 6, 1, 0, 120, 0},
        PlayerMatchStats{"match-2", "attacker", 3, 1, 0, 1, 20, 120},
    };
    MatchResult match2{"match-2", "defender", "attacker", now + std::chrono::seconds(1), match2_stats};
    service.RecordMatch(match2);

    std::vector<PlayerMatchStats> match3_stats{
        PlayerMatchStats{"match-3", "defender", 6, 6, 1, 0, 120, 0},
        PlayerMatchStats{"match-3", "attacker", 3, 1, 0, 1, 20, 120},
    };
    MatchResult match3{"match-3", "defender", "attacker", now + std::chrono::seconds(2), match3_stats};
    service.RecordMatch(match3);

    auto updated_attacker = service.GetProfile("attacker");
    ASSERT_TRUE(updated_attacker.has_value());
    EXPECT_EQ(3u, updated_attacker->matches);
    EXPECT_EQ(1u, updated_attacker->wins);
    EXPECT_EQ(2u, updated_attacker->losses);
    EXPECT_EQ(140u, updated_attacker->damage_dealt);
    EXPECT_EQ(260u, updated_attacker->damage_taken);
    EXPECT_EQ(1188, updated_attacker->rating);

    auto updated_defender = service.GetProfile("defender");
    ASSERT_TRUE(updated_defender.has_value());
    EXPECT_EQ(3u, updated_defender->matches);
    EXPECT_EQ(2u, updated_defender->wins);
    EXPECT_EQ(1u, updated_defender->losses);
    EXPECT_EQ(280u, updated_defender->damage_dealt);
    EXPECT_EQ(1213, updated_defender->rating);

    auto leaderboard_profiles = service.TopProfiles(2);
    ASSERT_EQ(2u, leaderboard_profiles.size());
    EXPECT_EQ("defender", leaderboard_profiles.front().player_id);
    EXPECT_EQ("attacker", leaderboard_profiles.back().player_id);

    const std::string leaderboard_json = service.SerializeLeaderboard(leaderboard_profiles);
    const auto defender_pos = leaderboard_json.find("defender");
    const auto attacker_pos = leaderboard_json.find("attacker");
    ASSERT_NE(defender_pos, std::string::npos);
    ASSERT_NE(attacker_pos, std::string::npos);
    EXPECT_LT(defender_pos, attacker_pos);

    const std::string metrics_after = service.MetricsSnapshot();
    EXPECT_NE(metrics_after.find("matches_recorded_total 3"), std::string::npos);
    EXPECT_NE(metrics_after.find("rating_updates_total 6"), std::string::npos);
}

