#include "arena60/matchmaking/match_notification_channel.h"

namespace arena60 {

void MatchNotificationChannel::Publish(const Match& match) {
    std::lock_guard<std::mutex> lk(mutex_);
    queue_.push(match);
}

std::optional<Match> MatchNotificationChannel::Poll() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }
    Match next = queue_.front();
    queue_.pop();
    return next;
}

std::vector<Match> MatchNotificationChannel::Drain() {
    std::vector<Match> matches;
    std::lock_guard<std::mutex> lk(mutex_);
    while (!queue_.empty()) {
        matches.push_back(queue_.front());
        queue_.pop();
    }
    return matches;
}

}  // namespace arena60
