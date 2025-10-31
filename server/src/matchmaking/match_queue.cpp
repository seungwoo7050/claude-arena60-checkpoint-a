#include "arena60/matchmaking/match_queue.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace arena60 {

InMemoryMatchQueue::InMemoryMatchQueue() = default;
InMemoryMatchQueue::~InMemoryMatchQueue() = default;

void InMemoryMatchQueue::Upsert(const MatchRequest& request, std::uint64_t order) {
    auto existing = index_.find(request.player_id());
    if (existing != index_.end()) {
        auto bucket_it = buckets_.find(existing->second.first);
        if (bucket_it != buckets_.end()) {
            bucket_it->second.erase(existing->second.second);
            if (bucket_it->second.empty()) {
                buckets_.erase(bucket_it);
            }
        }
        index_.erase(existing);
    }

    auto& bucket = buckets_[request.elo()];
    BucketEntry entry{request, order};
    auto insert_pos = bucket.end();
    for (auto it = bucket.begin(); it != bucket.end(); ++it) {
        if (order < it->order) {
            insert_pos = it;
            break;
        }
    }
    auto inserted = bucket.insert(insert_pos, std::move(entry));
    index_[inserted->request.player_id()] = {request.elo(), inserted};
}

bool InMemoryMatchQueue::Remove(const std::string& player_id) {
    auto existing = index_.find(player_id);
    if (existing == index_.end()) {
        return false;
    }
    auto bucket_it = buckets_.find(existing->second.first);
    if (bucket_it == buckets_.end()) {
        index_.erase(existing);
        return false;
    }
    bucket_it->second.erase(existing->second.second);
    if (bucket_it->second.empty()) {
        buckets_.erase(bucket_it);
    }
    index_.erase(existing);
    return true;
}

std::vector<QueuedPlayer> InMemoryMatchQueue::FetchOrdered() const {
    std::vector<QueuedPlayer> ordered;
    ordered.reserve(index_.size());
    for (const auto& bucket : buckets_) {
        for (const auto& entry : bucket.second) {
            ordered.push_back(QueuedPlayer{entry.request, entry.order});
        }
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const QueuedPlayer& lhs, const QueuedPlayer& rhs) {
                  if (lhs.request.elo() == rhs.request.elo()) {
                      return lhs.order < rhs.order;
                  }
                  return lhs.request.elo() < rhs.request.elo();
              });
    return ordered;
}

std::size_t InMemoryMatchQueue::Size() const { return index_.size(); }

std::string InMemoryMatchQueue::Snapshot() const {
    std::ostringstream oss;
    bool first = true;
    for (const auto& bucket : buckets_) {
        for (const auto& entry : bucket.second) {
            if (!first) {
                oss << ",";
            }
            first = false;
            oss << entry.request.player_id() << ':' << entry.request.elo();
        }
    }
    return oss.str();
}

RedisMatchQueue::RedisMatchQueue(std::ostream& stream) : stream_(&stream) {}

void RedisMatchQueue::Upsert(const MatchRequest& request, std::uint64_t order) {
    if (stream_) {
        (*stream_) << "ZADD matchmaking_queue " << request.elo() << ' ' << request.player_id()
                    << std::endl;
    }
    fallback_.Upsert(request, order);
}

bool RedisMatchQueue::Remove(const std::string& player_id) {
    if (stream_) {
        (*stream_) << "ZREM matchmaking_queue " << player_id << std::endl;
    }
    return fallback_.Remove(player_id);
}

std::vector<QueuedPlayer> RedisMatchQueue::FetchOrdered() const {
    if (stream_) {
        (*stream_) << "ZRANGE matchmaking_queue 0 -1 WITHSCORES" << std::endl;
    }
    return fallback_.FetchOrdered();
}

std::size_t RedisMatchQueue::Size() const { return fallback_.Size(); }

std::string RedisMatchQueue::Snapshot() const { return fallback_.Snapshot(); }

}  // namespace arena60
