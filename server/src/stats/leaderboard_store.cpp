#include "arena60/stats/leaderboard_store.h"

#include <algorithm>
#include <iostream>

namespace arena60 {

void InMemoryLeaderboardStore::RemoveFromOrdered(const std::string& player_id, int score) {
    auto ordered_it = ordered_.find(score);
    if (ordered_it == ordered_.end()) {
        return;
    }
    ordered_it->second.erase(player_id);
    if (ordered_it->second.empty()) {
        ordered_.erase(ordered_it);
    }
}

void InMemoryLeaderboardStore::Upsert(const std::string& player_id, int score) {
    const auto existing = scores_.find(player_id);
    if (existing != scores_.end()) {
        if (existing->second == score) {
            return;
        }
        RemoveFromOrdered(player_id, existing->second);
    }
    scores_[player_id] = score;
    ordered_[score].insert(player_id);
}

void InMemoryLeaderboardStore::Erase(const std::string& player_id) {
    const auto existing = scores_.find(player_id);
    if (existing == scores_.end()) {
        return;
    }
    RemoveFromOrdered(player_id, existing->second);
    scores_.erase(existing);
}

std::vector<std::pair<std::string, int>> InMemoryLeaderboardStore::TopN(std::size_t limit) const {
    std::vector<std::pair<std::string, int>> result;
    result.reserve(std::min(limit, scores_.size()));
    std::size_t remaining = limit;
    for (const auto& [score, players] : ordered_) {
        for (const auto& player : players) {
            if (remaining == 0) {
                return result;
            }
            result.emplace_back(player, score);
            --remaining;
        }
        if (remaining == 0) {
            break;
        }
    }
    return result;
}

std::optional<int> InMemoryLeaderboardStore::Get(const std::string& player_id) const {
    const auto it = scores_.find(player_id);
    if (it == scores_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::size_t InMemoryLeaderboardStore::Size() const { return scores_.size(); }

void RedisLeaderboardStore::Upsert(const std::string& player_id, int score) {
    std::cout << "redis zadd leaderboard " << score << ' ' << player_id << std::endl;
}

void RedisLeaderboardStore::Erase(const std::string& player_id) {
    std::cout << "redis zrem leaderboard " << player_id << std::endl;
}

std::vector<std::pair<std::string, int>> RedisLeaderboardStore::TopN(std::size_t limit) const {
    std::cout << "redis zrevrange leaderboard 0 " << (limit ? limit - 1 : 0) << " withscores"
              << std::endl;
    return {};
}

std::optional<int> RedisLeaderboardStore::Get(const std::string& player_id) const {
    std::cout << "redis zscore leaderboard " << player_id << std::endl;
    return std::nullopt;
}

std::size_t RedisLeaderboardStore::Size() const { return 0; }

}  // namespace arena60
