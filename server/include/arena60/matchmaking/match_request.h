#pragma once

#include <chrono>
#include <string>

namespace arena60 {

class MatchRequest {
   public:
    MatchRequest(std::string player_id, int elo,
                 std::chrono::steady_clock::time_point enqueued_at,
                 std::string preferred_region = "global");

    const std::string& player_id() const noexcept { return player_id_; }
    int elo() const noexcept { return elo_; }
    std::chrono::steady_clock::time_point enqueued_at() const noexcept { return enqueued_at_; }
    const std::string& preferred_region() const noexcept { return preferred_region_; }

    double WaitSeconds(std::chrono::steady_clock::time_point now) const noexcept;
    int CurrentTolerance(std::chrono::steady_clock::time_point now) const noexcept;

   private:
    std::string player_id_;
    int elo_;
    std::chrono::steady_clock::time_point enqueued_at_;
    std::string preferred_region_;
};

bool RegionsCompatible(const MatchRequest& lhs, const MatchRequest& rhs) noexcept;

}  // namespace arena60
