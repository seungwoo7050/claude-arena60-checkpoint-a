#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_set>

#include "arena60/matchmaking/matchmaker.h"

namespace {
using namespace std::chrono;
using arena60::InMemoryMatchQueue;
using arena60::Matchmaker;
using arena60::MatchRequest;
}  // namespace

TEST(MatchmakerFlowTest, ProducesMultipleMatchesAndNotifications) {
    auto queue = std::make_shared<InMemoryMatchQueue>();
    Matchmaker matchmaker(queue);
    const auto base = steady_clock::now() - seconds(20);

    for (int i = 0; i < 20; ++i) {
        const int elo = 1000 + (i % 10) * 10;
        matchmaker.Enqueue(MatchRequest{"player" + std::to_string(i), elo, base + milliseconds(i)});
    }

    auto matches = matchmaker.RunMatching(base + seconds(30));
    ASSERT_EQ(10u, matches.size());

    auto notifications = matchmaker.notification_channel().Drain();
    EXPECT_EQ(matches.size(), notifications.size());

    std::unordered_set<std::string> unique_players;
    for (const auto& match : matches) {
        ASSERT_EQ(2u, match.players().size());
        unique_players.insert(match.players()[0]);
        unique_players.insert(match.players()[1]);
    }
    EXPECT_EQ(20u, unique_players.size());
}
