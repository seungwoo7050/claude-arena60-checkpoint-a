#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace arena60 {

struct TickInfo {
    std::uint64_t tick;  // zero-based tick count
    double delta_seconds;
    std::chrono::steady_clock::time_point frame_start;
};

class GameLoop {
   public:
    explicit GameLoop(double tick_rate);
    ~GameLoop();

    void Start();
    void Stop();
    void Join();

    void SetUpdateCallback(std::function<void(const TickInfo&)> callback);

    double TargetDelta() const noexcept;
    double CurrentTickRate() const;
    std::vector<double> LastDurations() const;
    std::string PrometheusSnapshot() const;

   private:
    void Run();

    const double tick_rate_;
    const std::chrono::duration<double> target_delta_;

    std::function<void(const TickInfo&)> callback_;

    std::atomic<bool> running_{false};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable stop_cv_;
    bool stop_requested_{false};

    mutable std::mutex metrics_mutex_;
    std::vector<double> last_durations_;
    std::uint64_t tick_counter_{0};
};

}  // namespace arena60
