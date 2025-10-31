#include <gtest/gtest.h>

#include <cmath>

#include "arena60/game/game_session.h"

TEST(GameSessionTest, AppliesMovementWithSpeedClamp) {
    arena60::GameSession session(60.0);
    session.UpsertPlayer("p1");

    arena60::MovementInput input;
    input.sequence = 1;
    input.up = true;
    input.right = true;
    input.mouse_x = 1.0;
    input.mouse_y = 0.0;

    session.ApplyInput("p1", input, 1.0 / 60.0);

    const auto state = session.GetPlayer("p1");
    const double expected_distance = 5.0 / 60.0;
    EXPECT_NEAR(std::hypot(state.x, state.y), expected_distance, 1e-5);
    EXPECT_NEAR(state.facing_radians, 0.0, 1e-6);
}

TEST(GameSessionTest, RejectsOutOfOrderInputs) {
    arena60::GameSession session(60.0);
    session.UpsertPlayer("p1");

    arena60::MovementInput input;
    input.sequence = 2;
    input.up = true;
    session.ApplyInput("p1", input, 1.0 / 60.0);

    input.sequence = 1;
    input.up = false;
    session.ApplyInput("p1", input, 1.0 / 60.0);

    const auto state = session.GetPlayer("p1");
    EXPECT_GT(state.last_sequence, 1u);
}
