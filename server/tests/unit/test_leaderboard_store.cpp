#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "arena60/stats/leaderboard_store.h"

TEST(LeaderboardStoreTest, MaintainsDeterministicOrderingAndUpdates) {
    arena60::InMemoryLeaderboardStore store;
    store.Upsert("alice", 1200);
    store.Upsert("bob", 1300);
    store.Upsert("charlie", 1300);

    auto top_three = store.TopN(3);
    ASSERT_EQ(3u, top_three.size());
    EXPECT_EQ("bob", top_three[0].first);
    EXPECT_EQ(1300, top_three[0].second);
    EXPECT_EQ("charlie", top_three[1].first);
    EXPECT_EQ(1300, top_three[1].second);
    EXPECT_EQ("alice", top_three[2].first);

    auto alice_score = store.Get("alice");
    ASSERT_TRUE(alice_score.has_value());
    EXPECT_EQ(1200, *alice_score);

    store.Upsert("alice", 1400);
    auto top_one = store.TopN(1);
    ASSERT_EQ(1u, top_one.size());
    EXPECT_EQ("alice", top_one[0].first);
    EXPECT_EQ(1400, top_one[0].second);

    store.Erase("bob");
    auto remaining = store.TopN(5);
    ASSERT_EQ(2u, remaining.size());
    EXPECT_EQ("alice", remaining[0].first);
    EXPECT_EQ("charlie", remaining[1].first);
    EXPECT_EQ(2u, store.Size());
}

