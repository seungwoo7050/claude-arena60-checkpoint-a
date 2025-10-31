#include "arena60/game/game_session.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace arena60 {

namespace {
constexpr double kPlayerSpeed = 5.0;   // meters per second
constexpr double kPlayerRadius = 0.5;  // meters
constexpr double kFireCooldown = 0.1;  // seconds (10 shots per second)
constexpr double kSpawnOffset = 0.3;   // meters
constexpr int kDamagePerHit = 20;      // hit points per collision
}  // namespace

GameSession::GameSession(double /*tick_rate*/) : speed_per_second_(kPlayerSpeed), combat_log_(32) {}

void GameSession::UpsertPlayer(const std::string& player_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto& runtime = players_[player_id];
    if (runtime.state.player_id.empty()) {
        runtime.state.player_id = player_id;
        runtime.state.x = 0.0;
        runtime.state.y = 0.0;
        runtime.state.facing_radians = 0.0;
        runtime.state.last_sequence = 0;
        runtime.shots_fired = 0;
        runtime.hits_landed = 0;
        runtime.deaths = 0;
        runtime.state.shots_fired = 0;
        runtime.state.hits_landed = 0;
        runtime.state.deaths = 0;
    }
    runtime.health.Reset();
    runtime.state.health = runtime.health.current();
    runtime.state.is_alive = runtime.health.is_alive();
    runtime.death_announced = false;
    runtime.last_fire_time = std::numeric_limits<double>::lowest();
}

void GameSession::RemovePlayer(const std::string& player_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    players_.erase(player_id);
    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                                      [&](const Projectile& projectile) {
                                          return projectile.owner_id() == player_id;
                                      }),
                       projectiles_.end());
}

void GameSession::ApplyInput(const std::string& player_id, const MovementInput& input,
                             double delta_seconds) {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = players_.find(player_id);
    if (it == players_.end()) {
        return;
    }

    PlayerRuntimeState& runtime = it->second;
    PlayerState& state = runtime.state;
    if (input.sequence < state.last_sequence) {
        return;
    }
    state.last_sequence = input.sequence;

    state.facing_radians = std::atan2(input.mouse_y, input.mouse_x);

    if (state.is_alive) {
        double dx = 0.0;
        double dy = 0.0;
        if (input.up) {
            dy -= 1.0;
        }
        if (input.down) {
            dy += 1.0;
        }
        if (input.left) {
            dx -= 1.0;
        }
        if (input.right) {
            dx += 1.0;
        }

        double magnitude = std::sqrt(dx * dx + dy * dy);
        if (magnitude > 0.0) {
            dx /= magnitude;
            dy /= magnitude;
        }

        const double distance = speed_per_second_ * delta_seconds;
        state.x += dx * distance;
        state.y += dy * distance;
    }

    TrySpawnProjectile(runtime, input);
}

void GameSession::Tick(std::uint64_t tick, double delta_seconds) {
    std::lock_guard<std::mutex> lk(mutex_);
    UpdateProjectilesLocked(tick, delta_seconds);
}

PlayerState GameSession::GetPlayer(const std::string& player_id) const {
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = players_.find(player_id);
    if (it == players_.end()) {
        throw std::runtime_error("player not found");
    }
    return it->second.state;
}

std::vector<PlayerState> GameSession::Snapshot() const {
    std::lock_guard<std::mutex> lk(mutex_);
    std::vector<PlayerState> states;
    states.reserve(players_.size());
    for (const auto& kv : players_) {
        states.push_back(kv.second.state);
    }
    return states;
}

std::vector<CombatEvent> GameSession::ConsumeDeathEvents() {
    std::lock_guard<std::mutex> lk(mutex_);
    std::vector<CombatEvent> events = std::move(pending_deaths_);
    pending_deaths_.clear();
    return events;
}

std::vector<CombatEvent> GameSession::CombatLogSnapshot() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return combat_log_.Snapshot();
}

std::string GameSession::MetricsSnapshot() const {
    std::lock_guard<std::mutex> lk(mutex_);
    std::ostringstream oss;
    const auto active_projectiles =
        std::count_if(projectiles_.begin(), projectiles_.end(),
                      [](const Projectile& projectile) { return projectile.active(); });
    oss << "# TYPE projectiles_active gauge\n";
    oss << "projectiles_active " << active_projectiles << "\n";
    oss << "# TYPE projectiles_spawned_total counter\n";
    oss << "projectiles_spawned_total " << projectiles_spawned_total_ << "\n";
    oss << "# TYPE projectiles_hits_total counter\n";
    oss << "projectiles_hits_total " << projectiles_hits_total_ << "\n";
    oss << "# TYPE players_dead_total counter\n";
    oss << "players_dead_total " << players_dead_total_ << "\n";
    oss << "# TYPE collisions_checked_total counter\n";
    oss << "collisions_checked_total " << collisions_checked_total_ << "\n";
    return oss.str();
}

std::size_t GameSession::ActiveProjectileCount() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return std::count_if(projectiles_.begin(), projectiles_.end(),
                         [](const Projectile& projectile) { return projectile.active(); });
}

void GameSession::AppendCombatEvent(const CombatEvent& event) { combat_log_.Add(event); }

bool GameSession::TrySpawnProjectile(PlayerRuntimeState& runtime, const MovementInput& input) {
    if (!input.fire || !runtime.state.is_alive) {
        return false;
    }

    const double aim_magnitude =
        std::sqrt(input.mouse_x * input.mouse_x + input.mouse_y * input.mouse_y);
    if (aim_magnitude < 1e-6) {
        return false;
    }

    if ((elapsed_time_ - runtime.last_fire_time) < kFireCooldown) {
        return false;
    }

    runtime.last_fire_time = elapsed_time_;

    const double dir_x = input.mouse_x / aim_magnitude;
    const double dir_y = input.mouse_y / aim_magnitude;
    const double spawn_x = runtime.state.x + dir_x * kSpawnOffset;
    const double spawn_y = runtime.state.y + dir_y * kSpawnOffset;

    std::ostringstream id_stream;
    id_stream << "projectile-" << ++projectile_counter_;
    Projectile projectile(id_stream.str(), runtime.state.player_id, spawn_x, spawn_y, dir_x, dir_y,
                          elapsed_time_);
    std::cout << "projectile spawn " << projectile.id() << " owner=" << runtime.state.player_id
              << std::endl;
    projectiles_.push_back(std::move(projectile));
    ++projectiles_spawned_total_;
    ++runtime.shots_fired;
    runtime.state.shots_fired = runtime.shots_fired;
    return true;
}

void GameSession::UpdateProjectilesLocked(std::uint64_t tick, double delta_seconds) {
    elapsed_time_ += delta_seconds;

    for (auto& projectile : projectiles_) {
        projectile.Advance(delta_seconds);
        if (projectile.IsExpired(elapsed_time_)) {
            projectile.Deactivate();
        }
    }

    std::uint64_t pairs_checked = 0;
    for (auto& projectile : projectiles_) {
        if (!projectile.active()) {
            continue;
        }
        for (auto& kv : players_) {
            PlayerRuntimeState& runtime = kv.second;
            if (!runtime.state.is_alive || runtime.state.player_id == projectile.owner_id()) {
                continue;
            }
            ++pairs_checked;
            const double dx = projectile.x() - runtime.state.x;
            const double dy = projectile.y() - runtime.state.y;
            const double radius_sum = projectile.radius() + kPlayerRadius;
            if (std::abs(dx) > radius_sum || std::abs(dy) > radius_sum) {
                continue;
            }
            const double distance_sq = dx * dx + dy * dy;
            if (distance_sq <= radius_sum * radius_sum) {
                projectile.Deactivate();
                CombatEvent hit_event;
                hit_event.type = CombatEventType::Hit;
                hit_event.shooter_id = projectile.owner_id();
                hit_event.target_id = runtime.state.player_id;
                hit_event.projectile_id = projectile.id();
                hit_event.damage = kDamagePerHit;
                hit_event.tick = tick;
                AppendCombatEvent(hit_event);
                std::cout << "hit " << hit_event.shooter_id << "->" << hit_event.target_id
                          << " dmg=" << hit_event.damage << std::endl;
                ++projectiles_hits_total_;

                const bool died = runtime.health.ApplyDamage(kDamagePerHit);
                runtime.state.health = runtime.health.current();
                runtime.state.is_alive = runtime.health.is_alive();

                auto shooter_it = players_.find(projectile.owner_id());
                if (shooter_it != players_.end()) {
                    ++shooter_it->second.hits_landed;
                    shooter_it->second.state.hits_landed = shooter_it->second.hits_landed;
                }

                if (died && !runtime.death_announced) {
                    runtime.death_announced = true;
                    CombatEvent death_event;
                    death_event.type = CombatEventType::Death;
                    death_event.shooter_id = projectile.owner_id();
                    death_event.target_id = runtime.state.player_id;
                    death_event.projectile_id = projectile.id();
                    death_event.tick = tick;
                    pending_deaths_.push_back(death_event);
                    AppendCombatEvent(death_event);
                    ++players_dead_total_;
                    ++runtime.deaths;
                    runtime.state.deaths = runtime.deaths;
                    std::cout << "death " << runtime.state.player_id << std::endl;
                }
                break;
            }
        }
    }

    collisions_checked_total_ += pairs_checked;

    projectiles_.erase(
        std::remove_if(projectiles_.begin(), projectiles_.end(),
                       [](const Projectile& projectile) { return !projectile.active(); }),
        projectiles_.end());
}

}  // namespace arena60
