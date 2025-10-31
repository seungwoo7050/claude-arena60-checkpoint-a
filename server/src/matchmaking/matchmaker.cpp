#include "arena60/matchmaking/matchmaker.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace arena60 {

Matchmaker::Matchmaker(std::shared_ptr<MatchQueue> queue) : queue_(std::move(queue)) {}

void Matchmaker::SetMatchCreatedCallback(std::function<void(const Match&)> callback) {
    std::lock_guard<std::mutex> lk(mutex_);
    callback_ = std::move(callback);
}

void Matchmaker::Enqueue(const MatchRequest& request) {
    std::size_t queue_size = 0;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        queue_->Upsert(request, ++order_counter_);
        last_queue_size_ = queue_->Size();
        queue_size = last_queue_size_;
    }
    std::cout << "matchmaking enqueue " << request.player_id() << " elo=" << request.elo()
              << " size=" << queue_size << std::endl;
}

bool Matchmaker::Cancel(const std::string& player_id) {
    bool removed = false;
    std::size_t queue_size = 0;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        removed = queue_->Remove(player_id);
        last_queue_size_ = queue_->Size();
        queue_size = last_queue_size_;
    }
    if (removed) {
        std::cout << "matchmaking cancel " << player_id << " size=" << queue_size << std::endl;
    }
    return removed;
}

std::vector<Match> Matchmaker::RunMatching(std::chrono::steady_clock::time_point now) {
    std::vector<Match> matches;
    std::function<void(const Match&)> callback;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        auto ordered = queue_->FetchOrdered();
        std::unordered_set<std::string> used;
        for (std::size_t i = 0; i < ordered.size(); ++i) {
            const auto& candidate = ordered[i];
            const auto& request = candidate.request;
            if (used.find(request.player_id()) != used.end()) {
                continue;
            }
            const int tolerance_a = request.CurrentTolerance(now);
            std::size_t partner_index = ordered.size();
            for (std::size_t j = i + 1; j < ordered.size(); ++j) {
                const auto& other = ordered[j];
                if (used.find(other.request.player_id()) != used.end()) {
                    continue;
                }
                if (!RegionsCompatible(request, other.request)) {
                    continue;
                }
                const int diff = std::abs(request.elo() - other.request.elo());
                const int tolerance_b = other.request.CurrentTolerance(now);
                if (diff <= tolerance_a && diff <= tolerance_b) {
                    partner_index = j;
                    break;
                }
                if (other.request.elo() - request.elo() > tolerance_a) {
                    break;
                }
            }
            if (partner_index >= ordered.size()) {
                continue;
            }
            const auto& partner = ordered[partner_index].request;
            queue_->Remove(request.player_id());
            queue_->Remove(partner.player_id());
            used.insert(request.player_id());
            used.insert(partner.player_id());

            ++matches_created_;
            const int average_elo = (request.elo() + partner.elo()) / 2;
            std::ostringstream id_stream;
            id_stream << "match-" << ++match_counter_;
            Match match(id_stream.str(), {request.player_id(), partner.player_id()}, average_elo, now,
                        ResolveRegion(request, partner));
            matches.push_back(match);

            ObserveWaitLocked(request.WaitSeconds(now));
            ObserveWaitLocked(partner.WaitSeconds(now));
        }
        last_queue_size_ = queue_->Size();
        callback = callback_;
    }

    for (const auto& match : matches) {
        std::cout << "matchmaking match " << match.match_id() << " players=" << match.players()[0]
                  << ',' << match.players()[1] << " elo=" << match.average_elo() << std::endl;
        notifications_.Publish(match);
        if (callback) {
            callback(match);
        }
    }

    return matches;
}

std::string Matchmaker::MetricsSnapshot() const {
    std::lock_guard<std::mutex> lk(mutex_);
    std::ostringstream oss;
    oss << "# TYPE matchmaking_queue_size gauge\n";
    oss << "matchmaking_queue_size " << last_queue_size_ << "\n";
    oss << "# TYPE matchmaking_matches_total counter\n";
    oss << "matchmaking_matches_total " << matches_created_ << "\n";
    oss << "# TYPE matchmaking_wait_seconds histogram\n";
    std::uint64_t cumulative = 0;
    for (std::size_t i = 0; i < kWaitBuckets.size(); ++i) {
        cumulative += wait_bucket_counts_[i];
        oss << "matchmaking_wait_seconds_bucket{le=\"" << kWaitBuckets[i] << "\"} " << cumulative
            << "\n";
    }
    cumulative += wait_overflow_count_;
    oss << "matchmaking_wait_seconds_bucket{le=\"+Inf\"} " << cumulative << "\n";
    oss << "matchmaking_wait_seconds_sum " << wait_sum_ << "\n";
    oss << "matchmaking_wait_seconds_count " << wait_count_ << "\n";
    return oss.str();
}

void Matchmaker::ObserveWaitLocked(double seconds) {
    wait_sum_ += seconds;
    ++wait_count_;
    bool bucket_found = false;
    for (std::size_t i = 0; i < kWaitBuckets.size(); ++i) {
        if (seconds <= kWaitBuckets[i]) {
            ++wait_bucket_counts_[i];
            bucket_found = true;
            break;
        }
    }
    if (!bucket_found) {
        ++wait_overflow_count_;
    }
}

std::string Matchmaker::ResolveRegion(const MatchRequest& lhs, const MatchRequest& rhs) {
    if (lhs.preferred_region() == rhs.preferred_region()) {
        return lhs.preferred_region();
    }
    if (lhs.preferred_region() == "any") {
        return rhs.preferred_region();
    }
    if (rhs.preferred_region() == "any") {
        return lhs.preferred_region();
    }
    return lhs.preferred_region();
}

}  // namespace arena60
