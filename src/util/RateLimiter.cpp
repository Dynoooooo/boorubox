#include "util/RateLimiter.hpp"

#include <thread>

namespace boorubox {

RateLimiter::RateLimiter(std::chrono::milliseconds delay)
    : delay_(delay), last_(std::chrono::steady_clock::now() - delay) {}

void RateLimiter::wait() {
  std::unique_lock lock(mutex_);
  const auto now = std::chrono::steady_clock::now();
  const auto next = last_ + delay_;
  if (next > now) {
    std::this_thread::sleep_until(next);
  }
  last_ = std::chrono::steady_clock::now();
}

}  // namespace boorubox
