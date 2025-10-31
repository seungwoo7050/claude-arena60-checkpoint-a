#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "arena60/game/game_session.h"

TEST(ProjectilePerformanceTest, UpdatesWithinBudget) {
    arena60::GameSession session(60.0);

    // Spawn 32 projectiles using distinct players to bypass the fire-rate limiter.
    for (int i = 0; i < 32; ++i) {
        const std::string player_id = "shooter" + std::to_string(i);
        session.UpsertPlayer(player_id);
        arena60::MovementInput input;
        input.sequence = 1;
        input.mouse_x = 1.0;
        input.fire = true;
        session.ApplyInput(player_id, input, 1.0 / 60.0);
    }
    ASSERT_GE(session.ActiveProjectileCount(), 32u);

    std::uint64_t tick = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 120; ++i) {
        session.Tick(++tick, 1.0 / 60.0);
    }
    const auto end = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double, std::milli>(end - start);
    const double per_tick_ms = elapsed.count() / 120.0;

    EXPECT_LT(per_tick_ms, 0.5);
}
