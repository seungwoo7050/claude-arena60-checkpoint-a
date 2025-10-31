#include <gtest/gtest.h>

#include "arena60/game/game_session.h"
#include "arena60/game/projectile.h"

TEST(ProjectileTest, AdvanceMovesAlongDirection) {
    arena60::Projectile projectile("p1", "player1", 0.0, 0.0, 1.0, 0.0, 0.0);
    projectile.Advance(0.1);
    EXPECT_NEAR(projectile.x(), 3.0, 1e-6);
    EXPECT_NEAR(projectile.y(), 0.0, 1e-6);
}

TEST(ProjectileTest, ExpiresAfterLifetime) {
    arena60::Projectile projectile("p1", "player1", 0.0, 0.0, 0.0, 1.0, 0.0);
    EXPECT_FALSE(projectile.IsExpired(0.5));
    EXPECT_TRUE(projectile.IsExpired(1.6));
}

TEST(ProjectileTest, GameSessionRespectsFireRateLimit) {
    arena60::GameSession session(60.0);
    session.UpsertPlayer("shooter");

    arena60::MovementInput input;
    input.sequence = 1;
    input.mouse_x = 1.0;
    input.fire = true;
    session.ApplyInput("shooter", input, 1.0 / 60.0);
    EXPECT_EQ(session.ActiveProjectileCount(), 1u);

    // Immediate second fire should be rejected due to cooldown.
    input.sequence = 2;
    session.ApplyInput("shooter", input, 1.0 / 60.0);
    EXPECT_EQ(session.ActiveProjectileCount(), 1u);

    // Advance the simulation enough to allow another shot.
    session.Tick(1, 0.11);
    input.sequence = 3;
    session.ApplyInput("shooter", input, 1.0 / 60.0);
    EXPECT_EQ(session.ActiveProjectileCount(), 2u);
}
