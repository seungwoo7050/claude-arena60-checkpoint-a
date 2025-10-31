#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "arena60/core/game_loop.h"

using namespace std::chrono_literals;

TEST(TickVariancePerformanceTest, VarianceWithinOneMillisecond) {
  arena60::GameLoop loop(60.0);
  std::mutex mutex;
  std::condition_variable cv;
  std::vector<double> samples;

  loop.SetUpdateCallback([&](const arena60::TickInfo& info) {
    if (info.tick == 0) {
      return;
    }
    std::lock_guard<std::mutex> lk(mutex);
    samples.push_back(info.delta_seconds);
    if (samples.size() >= 120) {
      cv.notify_one();
    }
  });

  loop.Start();

  std::unique_lock<std::mutex> lk(mutex);
  ASSERT_TRUE(cv.wait_for(lk, 3s, [&]() { return samples.size() >= 120; }));
  loop.Stop();
  lk.unlock();
  loop.Join();

  if (samples.size() > 10) {
    samples.erase(samples.begin(), samples.begin() + 5);
  }

  ASSERT_FALSE(samples.empty());

  double mean = 0.0;
  for (double value : samples) {
    mean += value;
  }
  mean /= static_cast<double>(samples.size());

  double variance = 0.0;
  for (double value : samples) {
    const double diff = value - mean;
    variance += diff * diff;
  }
  variance /= static_cast<double>(samples.size());
  const double std_dev_ms = std::sqrt(variance) * 1000.0;
  EXPECT_LE(std_dev_ms, 1.0);
}
