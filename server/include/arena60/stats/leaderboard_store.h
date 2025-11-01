#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace arena60 {

class LeaderboardStore {
   public:
    virtual ~LeaderboardStore() = default;

    virtual void Upsert(const std::string& player_id, int score) = 0;
    virtual void Erase(const std::string& player_id) = 0;
    virtual std::vector<std::pair<std::string, int>> TopN(std::size_t limit) const = 0;
    virtual std::optional<int> Get(const std::string& player_id) const = 0;
    virtual std::size_t Size() const = 0;
};

class InMemoryLeaderboardStore : public LeaderboardStore {
   public:
    void Upsert(const std::string& player_id, int score) override;
    void Erase(const std::string& player_id) override;
    std::vector<std::pair<std::string, int>> TopN(std::size_t limit) const override;
    std::optional<int> Get(const std::string& player_id) const override;
    std::size_t Size() const override;

   private:
    void RemoveFromOrdered(const std::string& player_id, int score);

    std::unordered_map<std::string, int> scores_;
    std::map<int, std::set<std::string>, std::greater<int>> ordered_;
};

class RedisLeaderboardStore : public LeaderboardStore {
   public:
    void Upsert(const std::string& player_id, int score) override;
    void Erase(const std::string& player_id) override;
    std::vector<std::pair<std::string, int>> TopN(std::size_t limit) const override;
    std::optional<int> Get(const std::string& player_id) const override;
    std::size_t Size() const override;
};

}  // namespace arena60

