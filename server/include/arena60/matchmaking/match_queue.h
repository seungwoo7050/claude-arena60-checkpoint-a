#pragma once

#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "arena60/matchmaking/match_request.h"

namespace arena60 {

struct QueuedPlayer {
    MatchRequest request;
    std::uint64_t order{0};
};

class MatchQueue {
   public:
    virtual ~MatchQueue() = default;

    virtual void Upsert(const MatchRequest& request, std::uint64_t order) = 0;
    virtual bool Remove(const std::string& player_id) = 0;
    virtual std::vector<QueuedPlayer> FetchOrdered() const = 0;
    virtual std::size_t Size() const = 0;
    virtual std::string Snapshot() const = 0;
};

class InMemoryMatchQueue : public MatchQueue {
   public:
    InMemoryMatchQueue();
    ~InMemoryMatchQueue() override;

    void Upsert(const MatchRequest& request, std::uint64_t order) override;
    bool Remove(const std::string& player_id) override;
    std::vector<QueuedPlayer> FetchOrdered() const override;
    std::size_t Size() const override;
    std::string Snapshot() const override;

   private:
    struct BucketEntry {
        MatchRequest request;
        std::uint64_t order;
    };

    using Bucket = std::list<BucketEntry>;

    std::map<int, Bucket> buckets_;
    std::unordered_map<std::string, std::pair<int, typename Bucket::iterator>> index_;
};

class RedisMatchQueue : public MatchQueue {
   public:
    explicit RedisMatchQueue(std::ostream& stream = std::cout);

    void Upsert(const MatchRequest& request, std::uint64_t order) override;
    bool Remove(const std::string& player_id) override;
    std::vector<QueuedPlayer> FetchOrdered() const override;
    std::size_t Size() const override;
    std::string Snapshot() const override;

   private:
    std::ostream* stream_;
    InMemoryMatchQueue fallback_;
};

}  // namespace arena60
