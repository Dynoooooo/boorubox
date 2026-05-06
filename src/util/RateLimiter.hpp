#pragma once

#include <chrono>
#include <mutex>

namespace boorubox {

class RateLimiter {
 public:
  explicit RateLimiter(std::chrono::milliseconds delay);
  void wait();

 private:
  std::chrono::milliseconds delay_;
  std::chrono::steady_clock::time_point last_;
  std::mutex mutex_;
};

}  // namespace boorubox
