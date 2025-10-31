#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "arena60/game/movement.h"
#include "arena60/game/player_state.h"

namespace arena60 {

class GameSession {
 public:
  explicit GameSession(double tick_rate);

  void UpsertPlayer(const std::string& player_id);
  void RemovePlayer(const std::string& player_id);

  void ApplyInput(const std::string& player_id, const MovementInput& input,
                  double delta_seconds);

  PlayerState GetPlayer(const std::string& player_id) const;
  std::vector<PlayerState> Snapshot() const;

 private:
  double speed_per_second_;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, PlayerState> players_;
};

}  // namespace arena60
