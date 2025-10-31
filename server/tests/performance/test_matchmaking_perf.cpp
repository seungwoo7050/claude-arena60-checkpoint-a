#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>

#include "arena60/matchmaking/matchmaker.h"

namespace {
using namespace std::chrono;
using arena60::InMemoryMatchQueue;
using arena60::Matchmaker;
using arena60::MatchRequest;
}

TEST(MatchmakingPerformanceTest, MatchesTwoHundredPlayersUnderTwoMilliseconds) {
    auto queue = std::make_shared<InMemoryMatchQueue>();
    Matchmaker matchmaker(queue);
    const auto base = steady_clock::now() - seconds(30);

    for (int i = 0; i < 200; ++i) {
        const int elo = 1000 + (i % 40) * 5;
        matchmaker.Enqueue(MatchRequest{"perf" + std::to_string(i), elo,
                                        base + milliseconds(i)});
    }

    const auto start = steady_clock::now();
    auto matches = matchmaker.RunMatching(base + seconds(40));
    const auto end = steady_clock::now();

    const auto elapsed_us = duration_cast<microseconds>(end - start).count();
    EXPECT_EQ(100u, matches.size());
    EXPECT_LE(elapsed_us, 2000) << "Matchmaking took " << elapsed_us << " us";
}
