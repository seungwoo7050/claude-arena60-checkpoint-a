#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "arena60/matchmaking/match.h"
#include "arena60/matchmaking/match_notification_channel.h"
#include "arena60/matchmaking/match_queue.h"

namespace arena60 {

class Matchmaker {
   public:
    explicit Matchmaker(std::shared_ptr<MatchQueue> queue);

    void SetMatchCreatedCallback(std::function<void(const Match&)> callback);

    void Enqueue(const MatchRequest& request);
    bool Cancel(const std::string& player_id);

    std::vector<Match> RunMatching(std::chrono::steady_clock::time_point now);

    std::string MetricsSnapshot() const;

    MatchNotificationChannel& notification_channel() { return notifications_; }

   private:
    void ObserveWaitLocked(double seconds);
    static std::string ResolveRegion(const MatchRequest& lhs, const MatchRequest& rhs);

    std::shared_ptr<MatchQueue> queue_;
    mutable std::mutex mutex_;
    std::function<void(const Match&)> callback_;
    MatchNotificationChannel notifications_;

    std::uint64_t order_counter_{0};
    std::uint64_t match_counter_{0};
    std::uint64_t matches_created_{0};
    std::size_t last_queue_size_{0};

    static constexpr std::array<double, 6> kWaitBuckets{{0.0, 5.0, 10.0, 20.0, 40.0, 80.0}};
    std::array<std::uint64_t, kWaitBuckets.size()> wait_bucket_counts_{};
    std::uint64_t wait_overflow_count_{0};
    double wait_sum_{0.0};
    std::uint64_t wait_count_{0};
};

}  // namespace arena60
