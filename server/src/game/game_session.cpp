#include "arena60/game/game_session.h"

#include <cmath>
#include <stdexcept>

namespace arena60 {

namespace {
constexpr double kPlayerSpeed = 5.0;  // meters per second
}

GameSession::GameSession(double /*tick_rate*/)
    : speed_per_second_(kPlayerSpeed) {}

void GameSession::UpsertPlayer(const std::string& player_id) {
  std::lock_guard<std::mutex> lk(mutex_);
  auto& state = players_[player_id];
  state.player_id = player_id;
}

void GameSession::RemovePlayer(const std::string& player_id) {
  std::lock_guard<std::mutex> lk(mutex_);
  players_.erase(player_id);
}

void GameSession::ApplyInput(const std::string& player_id, const MovementInput& input,
                             double delta_seconds) {
  std::lock_guard<std::mutex> lk(mutex_);
  auto it = players_.find(player_id);
  if (it == players_.end()) {
    return;
  }

  PlayerState& state = it->second;
  if (input.sequence < state.last_sequence) {
    return;
  }
  state.last_sequence = input.sequence;

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

  state.facing_radians = std::atan2(input.mouse_y, input.mouse_x);
}

PlayerState GameSession::GetPlayer(const std::string& player_id) const {
  std::lock_guard<std::mutex> lk(mutex_);
  auto it = players_.find(player_id);
  if (it == players_.end()) {
    throw std::runtime_error("player not found");
  }
  return it->second;
}

std::vector<PlayerState> GameSession::Snapshot() const {
  std::lock_guard<std::mutex> lk(mutex_);
  std::vector<PlayerState> states;
  states.reserve(players_.size());
  for (const auto& kv : players_) {
    states.push_back(kv.second);
  }
  return states;
}

}  // namespace arena60
