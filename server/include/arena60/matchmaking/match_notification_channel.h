#pragma once

#include <mutex>
#include <optional>
#include <queue>
#include <vector>

#include "arena60/matchmaking/match.h"

namespace arena60 {

class MatchNotificationChannel {
   public:
    void Publish(const Match& match);
    std::optional<Match> Poll();
    std::vector<Match> Drain();

   private:
    std::mutex mutex_;
    std::queue<Match> queue_;
};

}  // namespace arena60
