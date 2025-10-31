#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace arena60 {

class Match {
   public:
    Match(std::string match_id, std::vector<std::string> players,
          int average_elo, std::chrono::steady_clock::time_point created_at,
          std::string region);

    const std::string& match_id() const noexcept { return match_id_; }
    const std::vector<std::string>& players() const noexcept { return players_; }
    int average_elo() const noexcept { return average_elo_; }
    std::chrono::steady_clock::time_point created_at() const noexcept { return created_at_; }
    const std::string& region() const noexcept { return region_; }

   private:
    std::string match_id_;
    std::vector<std::string> players_;
    int average_elo_;
    std::chrono::steady_clock::time_point created_at_;
    std::string region_;
};

}  // namespace arena60
