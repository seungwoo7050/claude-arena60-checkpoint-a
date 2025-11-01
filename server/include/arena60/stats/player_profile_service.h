#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "arena60/stats/leaderboard_store.h"
#include "arena60/stats/match_stats.h"

namespace arena60 {

struct EloRatingUpdate {
    int winner_new{0};
    int loser_new{0};
};

class EloRatingCalculator {
   public:
    EloRatingUpdate Update(int winner_rating, int loser_rating) const;
};

struct PlayerProfile {
    std::string player_id;
    int rating{1200};
    std::uint64_t matches{0};
    std::uint64_t wins{0};
    std::uint64_t losses{0};
    std::uint64_t kills{0};
    std::uint64_t deaths{0};
    std::uint64_t shots_fired{0};
    std::uint64_t hits_landed{0};
    std::uint64_t damage_dealt{0};
    std::uint64_t damage_taken{0};

    double Accuracy() const noexcept;
};

class PlayerProfileService {
   public:
    explicit PlayerProfileService(std::shared_ptr<LeaderboardStore> leaderboard_store);

    void RecordMatch(const MatchResult& result);

    std::optional<PlayerProfile> GetProfile(const std::string& player_id) const;
    std::vector<PlayerProfile> TopProfiles(std::size_t limit) const;

    std::string SerializeProfile(const PlayerProfile& profile) const;
    std::string SerializeLeaderboard(const std::vector<PlayerProfile>& profiles) const;

    std::string MetricsSnapshot() const;

   private:
    struct AggregateStats {
        std::uint64_t matches{0};
        std::uint64_t wins{0};
        std::uint64_t losses{0};
        std::uint64_t kills{0};
        std::uint64_t deaths{0};
        std::uint64_t shots_fired{0};
        std::uint64_t hits_landed{0};
        std::uint64_t damage_dealt{0};
        std::uint64_t damage_taken{0};
        int rating{1200};
    };

    PlayerProfile BuildProfileUnsafe(const std::string& player_id, const AggregateStats& stats) const;

    std::shared_ptr<LeaderboardStore> leaderboard_;
    EloRatingCalculator calculator_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, AggregateStats> aggregates_;
    std::uint64_t matches_recorded_total_{0};
    std::uint64_t rating_updates_total_{0};
};

}  // namespace arena60

