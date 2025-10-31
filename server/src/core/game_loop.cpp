#include "arena60/core/game_loop.h"

#include <cmath>
#include <iostream>
#include <sstream>

namespace arena60 {

GameLoop::GameLoop(double tick_rate)
    : tick_rate_(tick_rate),
      target_delta_(std::chrono::duration<double>(1.0 / tick_rate)) {}

GameLoop::~GameLoop() { Stop(); Join(); }

void GameLoop::Start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return;
  }
  {
    std::lock_guard<std::mutex> lk(metrics_mutex_);
    last_durations_.clear();
    tick_counter_ = 0;
  }
  stop_requested_ = false;
  thread_ = std::thread([this]() { Run(); });
}

void GameLoop::Stop() {
  if (!running_) {
    return;
  }
  {
    std::lock_guard<std::mutex> lk(mutex_);
    stop_requested_ = true;
  }
  stop_cv_.notify_all();
}

void GameLoop::Join() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

void GameLoop::SetUpdateCallback(std::function<void(const TickInfo&)> callback) {
  std::lock_guard<std::mutex> lk(mutex_);
  callback_ = std::move(callback);
}

double GameLoop::TargetDelta() const noexcept { return target_delta_.count(); }

double GameLoop::CurrentTickRate() const {
  std::lock_guard<std::mutex> lk(metrics_mutex_);
  if (last_durations_.empty()) {
    return tick_rate_;
  }
  const double latest = last_durations_.back();
  if (latest == 0.0) {
    return tick_rate_;
  }
  return 1.0 / latest;
}

std::vector<double> GameLoop::LastDurations() const {
  std::lock_guard<std::mutex> lk(metrics_mutex_);
  return last_durations_;
}

std::string GameLoop::PrometheusSnapshot() const {
  std::ostringstream oss;
  oss << "# TYPE game_tick_rate gauge\n";
  oss << "game_tick_rate " << CurrentTickRate() << "\n";
  double last_duration = TargetDelta();
  {
    std::lock_guard<std::mutex> lk(metrics_mutex_);
    if (!last_durations_.empty()) {
      last_duration = last_durations_.back();
    }
  }
  oss << "# TYPE game_tick_duration_seconds gauge\n";
  oss << "game_tick_duration_seconds " << last_duration << "\n";
  return oss.str();
}

void GameLoop::Run() {
  auto previous = std::chrono::steady_clock::now();
  auto next_frame = previous + target_delta_;
  while (true) {
    {
      std::unique_lock<std::mutex> lk(mutex_);
      if (stop_requested_) {
        running_ = false;
        break;
      }
    }

    const auto frame_start = std::chrono::steady_clock::now();
    const auto elapsed = frame_start - previous;

    const double delta_seconds = std::chrono::duration<double>(elapsed).count();
    const TickInfo info{tick_counter_, delta_seconds, frame_start};
    {
      std::lock_guard<std::mutex> lk(mutex_);
      if (callback_) {
        callback_(info);
      }
    }

    previous = frame_start;
    next_frame += target_delta_;

    {
      std::lock_guard<std::mutex> lk(metrics_mutex_);
      last_durations_.push_back(delta_seconds);
      if (last_durations_.size() > 240) {
        last_durations_.erase(last_durations_.begin());
      }
      ++tick_counter_;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto sleep_duration = next_frame - now;
    if (sleep_duration.count() > 0) {
      std::unique_lock<std::mutex> lk(mutex_);
      stop_cv_.wait_for(lk, sleep_duration, [this]() { return stop_requested_; });
    } else {
      next_frame = std::chrono::steady_clock::now();
    }
  }
}

}  // namespace arena60
