#include "arena60/stats/player_profile_service.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace arena60 {

EloRatingUpdate EloRatingCalculator::Update(int winner_rating, int loser_rating) const {
    const double expected_winner =
        1.0 / (1.0 + std::pow(10.0, (loser_rating - winner_rating) / 400.0));
    const double expected_loser =
        1.0 / (1.0 + std::pow(10.0, (winner_rating - loser_rating) / 400.0));
    constexpr double kFactor = 25.0;
    const int winner_new =
        static_cast<int>(std::lround(winner_rating + kFactor * (1.0 - expected_winner)));
    const int loser_new =
        static_cast<int>(std::lround(loser_rating + kFactor * (0.0 - expected_loser)));
    return {winner_new, loser_new};
}

double PlayerProfile::Accuracy() const noexcept {
    if (shots_fired == 0) {
        return 0.0;
    }
    return static_cast<double>(hits_landed) / static_cast<double>(shots_fired);
}

PlayerProfileService::PlayerProfileService(std::shared_ptr<LeaderboardStore> leaderboard_store)
    : leaderboard_(std::move(leaderboard_store)) {}

void PlayerProfileService::RecordMatch(const MatchResult& result) {
    std::lock_guard<std::mutex> lk(mutex_);

    for (const auto& stats : result.player_stats()) {
        auto& aggregate = aggregates_[stats.player_id()];
        aggregate.matches += 1;
        aggregate.shots_fired += stats.shots_fired();
        aggregate.hits_landed += stats.hits_landed();
        aggregate.damage_dealt += stats.damage_dealt();
        aggregate.damage_taken += stats.damage_taken();
        aggregate.kills += stats.kills();
        aggregate.deaths += stats.deaths();
    }

    auto& winner = aggregates_[result.winner_id()];
    auto& loser = aggregates_[result.loser_id()];
    winner.wins += 1;
    loser.losses += 1;

    const auto update = calculator_.Update(winner.rating, loser.rating);
    winner.rating = update.winner_new;
    loser.rating = update.loser_new;
    rating_updates_total_ += 2;

    if (leaderboard_) {
        leaderboard_->Upsert(result.winner_id(), winner.rating);
        leaderboard_->Upsert(result.loser_id(), loser.rating);
    }

    ++matches_recorded_total_;
}

std::optional<PlayerProfile> PlayerProfileService::GetProfile(const std::string& player_id) const {
    std::lock_guard<std::mutex> lk(mutex_);
    const auto it = aggregates_.find(player_id);
    if (it == aggregates_.end()) {
        return std::nullopt;
    }
    return BuildProfileUnsafe(player_id, it->second);
}

std::vector<PlayerProfile> PlayerProfileService::TopProfiles(std::size_t limit) const {
    std::lock_guard<std::mutex> lk(mutex_);
    std::vector<PlayerProfile> profiles;
    if (!leaderboard_) {
        profiles.reserve(std::min(limit, aggregates_.size()));
        for (const auto& kv : aggregates_) {
            profiles.push_back(BuildProfileUnsafe(kv.first, kv.second));
        }
        std::sort(profiles.begin(), profiles.end(),
                  [](const PlayerProfile& lhs, const PlayerProfile& rhs) {
                      if (lhs.rating != rhs.rating) {
                          return lhs.rating > rhs.rating;
                      }
                      return lhs.player_id < rhs.player_id;
                  });
        if (profiles.size() > limit) {
            profiles.resize(limit);
        }
        return profiles;
    }

    const auto ordered = leaderboard_->TopN(limit);
    profiles.reserve(ordered.size());
    for (const auto& entry : ordered) {
        const auto it = aggregates_.find(entry.first);
        if (it != aggregates_.end()) {
            profiles.push_back(BuildProfileUnsafe(entry.first, it->second));
        }
    }
    return profiles;
}

std::string PlayerProfileService::SerializeProfile(const PlayerProfile& profile) const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"player_id\":\"" << profile.player_id << "\",";
    oss << "\"rating\":" << profile.rating << ",";
    oss << "\"matches\":" << profile.matches << ",";
    oss << "\"wins\":" << profile.wins << ",";
    oss << "\"losses\":" << profile.losses << ",";
    oss << "\"kills\":" << profile.kills << ",";
    oss << "\"deaths\":" << profile.deaths << ",";
    oss << "\"shots_fired\":" << profile.shots_fired << ",";
    oss << "\"hits_landed\":" << profile.hits_landed << ",";
    oss << "\"damage_dealt\":" << profile.damage_dealt << ",";
    oss << "\"damage_taken\":" << profile.damage_taken << ",";
    oss << "\"accuracy\":" << std::fixed << std::setprecision(4) << profile.Accuracy();
    oss << "}";
    return oss.str();
}

std::string PlayerProfileService::SerializeLeaderboard(
    const std::vector<PlayerProfile>& profiles) const {
    std::ostringstream oss;
    oss << "[";
    for (std::size_t i = 0; i < profiles.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << SerializeProfile(profiles[i]);
    }
    oss << "]";
    return oss.str();
}

std::string PlayerProfileService::MetricsSnapshot() const {
    std::lock_guard<std::mutex> lk(mutex_);
    std::ostringstream oss;
    oss << "# TYPE player_profiles_total gauge\n";
    oss << "player_profiles_total " << aggregates_.size() << "\n";
    const std::size_t leaderboard_size = leaderboard_ ? leaderboard_->Size() : aggregates_.size();
    oss << "# TYPE leaderboard_entries_total gauge\n";
    oss << "leaderboard_entries_total " << leaderboard_size << "\n";
    oss << "# TYPE matches_recorded_total counter\n";
    oss << "matches_recorded_total " << matches_recorded_total_ << "\n";
    oss << "# TYPE rating_updates_total counter\n";
    oss << "rating_updates_total " << rating_updates_total_ << "\n";
    return oss.str();
}

PlayerProfile PlayerProfileService::BuildProfileUnsafe(const std::string& player_id,
                                                       const AggregateStats& stats) const {
    PlayerProfile profile;
    profile.player_id = player_id;
    profile.rating = stats.rating;
    profile.matches = stats.matches;
    profile.wins = stats.wins;
    profile.losses = stats.losses;
    profile.kills = stats.kills;
    profile.deaths = stats.deaths;
    profile.shots_fired = stats.shots_fired;
    profile.hits_landed = stats.hits_landed;
    profile.damage_dealt = stats.damage_dealt;
    profile.damage_taken = stats.damage_taken;
    return profile;
}

}  // namespace arena60
