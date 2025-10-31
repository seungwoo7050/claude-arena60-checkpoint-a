#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

#include "arena60/core/game_loop.h"

using namespace std::chrono_literals;

TEST(GameLoopTest, TickRateIsCloseToTarget) {
    arena60::GameLoop loop(60.0);
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<double> deltas;

    loop.SetUpdateCallback([&](const arena60::TickInfo& info) {
        if (info.tick == 0) {
            return;
        }
        std::lock_guard<std::mutex> lk(mutex);
        deltas.push_back(info.delta_seconds);
        if (deltas.size() >= 8) {
            cv.notify_one();
        }
    });

    loop.Start();

    std::unique_lock<std::mutex> lk(mutex);
    ASSERT_TRUE(cv.wait_for(lk, 500ms, [&]() { return deltas.size() >= 8; }));
    loop.Stop();
    loop.Join();

    const double target = loop.TargetDelta();
    ASSERT_GE(deltas.size(), 2u);
    for (std::size_t i = 1; i < deltas.size(); ++i) {
        EXPECT_NEAR(deltas[i], target, 0.01);
    }

    const auto snapshot = loop.PrometheusSnapshot();
    EXPECT_NE(snapshot.find("game_tick_rate"), std::string::npos);
    EXPECT_NE(snapshot.find("game_tick_duration_seconds"), std::string::npos);
}

TEST(GameLoopTest, StopPreventsAdditionalTicks) {
    arena60::GameLoop loop(60.0);
    std::mutex mutex;
    std::condition_variable cv;
    std::size_t tick_count = 0;

    loop.SetUpdateCallback([&](const arena60::TickInfo& /*info*/) {
        std::lock_guard<std::mutex> lk(mutex);
        ++tick_count;
        if (tick_count == 5) {
            cv.notify_one();
        }
    });

    loop.Start();

    std::unique_lock<std::mutex> lk(mutex);
    ASSERT_TRUE(cv.wait_for(lk, 500ms, [&]() { return tick_count >= 5; }));
    loop.Stop();
    lk.unlock();
    loop.Join();

    std::this_thread::sleep_for(50ms);
    std::lock_guard<std::mutex> lock_guard(mutex);
    EXPECT_EQ(tick_count, 5);
}
