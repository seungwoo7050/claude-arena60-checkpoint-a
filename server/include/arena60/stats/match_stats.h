#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "arena60/game/combat.h"
#include "arena60/game/game_session.h"

namespace arena60 {

class PlayerMatchStats {
   public:
    PlayerMatchStats(std::string match_id, std::string player_id, std::uint32_t shots_fired,
                     std::uint32_t hits_landed, std::uint32_t kills, std::uint32_t deaths,
                     std::uint64_t damage_dealt, std::uint64_t damage_taken);

    const std::string& match_id() const noexcept { return match_id_; }
    const std::string& player_id() const noexcept { return player_id_; }
    std::uint32_t shots_fired() const noexcept { return shots_fired_; }
    std::uint32_t hits_landed() const noexcept { return hits_landed_; }
    std::uint32_t kills() const noexcept { return kills_; }
    std::uint32_t deaths() const noexcept { return deaths_; }
    std::uint64_t damage_dealt() const noexcept { return damage_dealt_; }
    std::uint64_t damage_taken() const noexcept { return damage_taken_; }

    double Accuracy() const noexcept;

   private:
    std::string match_id_;
    std::string player_id_;
    std::uint32_t shots_fired_{0};
    std::uint32_t hits_landed_{0};
    std::uint32_t kills_{0};
    std::uint32_t deaths_{0};
    std::uint64_t damage_dealt_{0};
    std::uint64_t damage_taken_{0};
};

class MatchResult {
   public:
    MatchResult(std::string match_id, std::string winner_id, std::string loser_id,
                std::chrono::system_clock::time_point completed_at,
                std::vector<PlayerMatchStats> player_stats);

    const std::string& match_id() const noexcept { return match_id_; }
    const std::string& winner_id() const noexcept { return winner_id_; }
    const std::string& loser_id() const noexcept { return loser_id_; }
    std::chrono::system_clock::time_point completed_at() const noexcept {
        return completed_at_;
    }
    const std::vector<PlayerMatchStats>& player_stats() const noexcept { return player_stats_; }

   private:
    std::string match_id_;
    std::string winner_id_;
    std::string loser_id_;
    std::chrono::system_clock::time_point completed_at_;
    std::vector<PlayerMatchStats> player_stats_;
};

class MatchStatsCollector {
   public:
    MatchResult Collect(const CombatEvent& death_event, const GameSession& session,
                        std::chrono::system_clock::time_point completed_at) const;
};

}  // namespace arena60

