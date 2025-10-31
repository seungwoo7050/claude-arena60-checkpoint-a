#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "arena60/matchmaking/matchmaker.h"

namespace {
using namespace std::chrono;
using arena60::InMemoryMatchQueue;
using arena60::Match;
using arena60::Matchmaker;
using arena60::MatchRequest;
}

TEST(MatchmakerTest, DoesNotMatchOutsideTolerance) {
    auto queue = std::make_shared<InMemoryMatchQueue>();
    Matchmaker matchmaker(queue);
    const auto now = steady_clock::now();
    matchmaker.Enqueue(MatchRequest{"alice", 1200, now});
    matchmaker.Enqueue(MatchRequest{"bob", 1350, now});

    auto matches = matchmaker.RunMatching(now);
    EXPECT_TRUE(matches.empty());
}

TEST(MatchmakerTest, MatchesAndEmitsMetricsWhenToleranceSatisfied) {
    auto queue = std::make_shared<InMemoryMatchQueue>();
    Matchmaker matchmaker(queue);
    const auto now = steady_clock::now();
    std::vector<Match> delivered;
    matchmaker.SetMatchCreatedCallback([&](const Match& match) { delivered.push_back(match); });

    matchmaker.Enqueue(MatchRequest{"alice", 1200, now - seconds(12)});
    matchmaker.Enqueue(MatchRequest{"bob", 1340, now - seconds(12)});

    auto matches = matchmaker.RunMatching(now);
    ASSERT_EQ(1u, matches.size());
    EXPECT_EQ("alice", matches[0].players()[0]);
    EXPECT_EQ("bob", matches[0].players()[1]);
    EXPECT_EQ(1u, delivered.size());

    auto notification = matchmaker.notification_channel().Poll();
    ASSERT_TRUE(notification.has_value());
    EXPECT_EQ(matches[0].match_id(), notification->match_id());

    const auto metrics = matchmaker.MetricsSnapshot();
    EXPECT_NE(metrics.find("matchmaking_queue_size 0"), std::string::npos);
    EXPECT_NE(metrics.find("matchmaking_matches_total 1"), std::string::npos);
    EXPECT_NE(metrics.find("matchmaking_wait_seconds_count 2"), std::string::npos);
}

TEST(MatchmakerTest, CancelRemovesPlayer) {
    auto queue = std::make_shared<InMemoryMatchQueue>();
    Matchmaker matchmaker(queue);
    const auto now = steady_clock::now();
    matchmaker.Enqueue(MatchRequest{"alice", 1200, now});
    EXPECT_TRUE(matchmaker.Cancel("alice"));
    EXPECT_FALSE(matchmaker.Cancel("alice"));
}
