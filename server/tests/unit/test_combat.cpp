#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>
#include <string>

#include "arena60/game/game_session.h"

namespace {
void MoveTarget(arena60::GameSession& session, const std::string& player_id, double delta_seconds) {
    arena60::MovementInput move;
    move.sequence = 1;
    move.right = true;
    move.mouse_x = 1.0;
    session.ApplyInput(player_id, move, delta_seconds);
}

void Fire(arena60::GameSession& session, const std::string& player_id, std::uint64_t sequence) {
    arena60::MovementInput input;
    input.sequence = sequence;
    input.mouse_x = 1.0;
    input.fire = true;
    session.ApplyInput(player_id, input, 1.0 / 60.0);
}

void AdvanceUntilNoProjectiles(arena60::GameSession& session, std::uint64_t& tick_counter,
                               double delta_seconds) {
    for (int i = 0; i < 120 && session.ActiveProjectileCount() > 0; ++i) {
        session.Tick(++tick_counter, delta_seconds);
    }
}
}  // namespace

TEST(GameCombatTest, ProjectileHitReducesHealth) {
    arena60::GameSession session(60.0);
    session.UpsertPlayer("attacker");
    session.UpsertPlayer("defender");

    MoveTarget(session, "defender", 0.08);  // place defender ~0.4m away

    std::uint64_t tick = 0;
    Fire(session, "attacker", 1);
    AdvanceUntilNoProjectiles(session, tick, 1.0 / 60.0);

    const auto defender = session.GetPlayer("defender");
    EXPECT_EQ(defender.health, 80);
    EXPECT_TRUE(defender.is_alive);

    const auto log = session.CombatLogSnapshot();
    ASSERT_FALSE(log.empty());
    EXPECT_EQ(log.back().type, arena60::CombatEventType::Hit);
}

TEST(GameCombatTest, DeathEventQueuedOnce) {
    arena60::GameSession session(60.0);
    session.UpsertPlayer("attacker");
    session.UpsertPlayer("defender");

    MoveTarget(session, "defender", 0.08);

    std::uint64_t tick = 0;
    for (int shot = 0; shot < 5; ++shot) {
        Fire(session, "attacker", shot + 1);
        AdvanceUntilNoProjectiles(session, tick, 1.0 / 60.0);
        session.Tick(++tick, 0.11);  // ensure cooldown before next shot
    }

    auto events = session.ConsumeDeathEvents();
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events.front().type, arena60::CombatEventType::Death);
    EXPECT_EQ(events.front().target_id, "defender");

    auto defender = session.GetPlayer("defender");
    EXPECT_EQ(defender.health, 0);
    EXPECT_FALSE(defender.is_alive);
    EXPECT_EQ(defender.deaths, 1);

    auto log = session.CombatLogSnapshot();
    ASSERT_GE(log.size(), 6u);  // 5 hits + 1 death
    EXPECT_EQ(std::count_if(log.begin(), log.end(),
                            [](const arena60::CombatEvent& ev) {
                                return ev.type == arena60::CombatEventType::Death;
                            }),
              1);
    EXPECT_TRUE(session.ConsumeDeathEvents().empty());

    const auto attacker = session.GetPlayer("attacker");
    EXPECT_GE(attacker.shots_fired, 5);
    EXPECT_GE(attacker.hits_landed, 5);
    EXPECT_EQ(attacker.deaths, 0);
}

TEST(GameCombatTest, MetricsIncludeCollisionChecks) {
    arena60::GameSession session(60.0);
    session.UpsertPlayer("attacker");
    session.UpsertPlayer("defender");

    arena60::MovementInput input;
    input.sequence = 1;
    input.mouse_x = 1.0;
    input.fire = true;
    session.ApplyInput("attacker", input, 1.0 / 60.0);
    ASSERT_GE(session.ActiveProjectileCount(), 1u);

    session.Tick(1, 1.0 / 60.0);

    const std::string metrics = session.MetricsSnapshot();
    std::istringstream iss(metrics);
    std::string line;
    bool found = false;
    std::uint64_t value = 0;
    while (std::getline(iss, line)) {
        if (line.rfind("collisions_checked_total ", 0) == 0) {
            value = std::stoull(line.substr(std::string("collisions_checked_total ").size()));
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_GE(value, 1u);
}
