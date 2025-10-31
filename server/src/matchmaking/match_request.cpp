#include "arena60/matchmaking/match_request.h"

#include <algorithm>

namespace arena60 {

namespace {
constexpr int kBaseTolerance = 100;
constexpr int kToleranceStep = 25;
constexpr double kStepSeconds = 5.0;
}

MatchRequest::MatchRequest(std::string player_id, int elo,
                           std::chrono::steady_clock::time_point enqueued_at,
                           std::string preferred_region)
    : player_id_(std::move(player_id)),
      elo_(elo),
      enqueued_at_(enqueued_at),
      preferred_region_(std::move(preferred_region)) {}

double MatchRequest::WaitSeconds(std::chrono::steady_clock::time_point now) const noexcept {
    return std::chrono::duration<double>(now - enqueued_at_).count();
}

int MatchRequest::CurrentTolerance(std::chrono::steady_clock::time_point now) const noexcept {
    const double waited = std::max(0.0, WaitSeconds(now));
    const int increments = static_cast<int>(waited / kStepSeconds);
    return kBaseTolerance + increments * kToleranceStep;
}

bool RegionsCompatible(const MatchRequest& lhs, const MatchRequest& rhs) noexcept {
    if (lhs.preferred_region() == "any" || rhs.preferred_region() == "any") {
        return true;
    }
    return lhs.preferred_region() == rhs.preferred_region();
}

}  // namespace arena60
