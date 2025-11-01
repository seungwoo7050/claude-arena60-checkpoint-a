#include <gtest/gtest.h>

#include <chrono>

#include "arena60/matchmaking/match_queue.h"

namespace {
using namespace std::chrono;
using arena60::InMemoryMatchQueue;
using arena60::MatchQueue;
using arena60::MatchRequest;
}  // namespace

TEST(MatchQueueTest, OrdersByEloAndInsertion) {
    InMemoryMatchQueue queue;
    const auto now = steady_clock::now();
    queue.Upsert(MatchRequest{"alice", 1200, now}, 1);
    queue.Upsert(MatchRequest{"bob", 1100, now}, 2);
    queue.Upsert(MatchRequest{"carol", 1200, now + milliseconds(10)}, 3);

    const auto ordered = queue.FetchOrdered();
    ASSERT_EQ(3u, ordered.size());
    EXPECT_EQ("bob", ordered[0].request.player_id());
    EXPECT_EQ("alice", ordered[1].request.player_id());
    EXPECT_EQ("carol", ordered[2].request.player_id());
}

TEST(MatchQueueTest, UpsertRefreshesExistingPlayer) {
    InMemoryMatchQueue queue;
    const auto now = steady_clock::now();
    queue.Upsert(MatchRequest{"alice", 1200, now}, 1);
    queue.Upsert(MatchRequest{"alice", 1250, now + seconds(2)}, 2);

    const auto ordered = queue.FetchOrdered();
    ASSERT_EQ(1u, ordered.size());
    EXPECT_EQ(1250, ordered.front().request.elo());
    EXPECT_NEAR(0.0, ordered.front().request.WaitSeconds(now + seconds(2)), 1e-6);
}

TEST(MatchQueueTest, RemoveDeletesPlayer) {
    InMemoryMatchQueue queue;
    const auto now = steady_clock::now();
    queue.Upsert(MatchRequest{"alice", 1200, now}, 1);
    queue.Upsert(MatchRequest{"bob", 1250, now}, 2);

    EXPECT_TRUE(queue.Remove("alice"));
    EXPECT_FALSE(queue.Remove("alice"));

    const auto ordered = queue.FetchOrdered();
    ASSERT_EQ(1u, ordered.size());
    EXPECT_EQ("bob", ordered.front().request.player_id());
}
