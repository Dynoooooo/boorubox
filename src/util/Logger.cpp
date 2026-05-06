#include "util/Logger.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace boorubox {

void Logger::info(std::string message) {
  push("info", std::move(message));
}

void Logger::warn(std::string message) {
  push("warn", std::move(message));
}

void Logger::error(std::string message) {
  push("error", std::move(message));
}

std::vector<std::string> Logger::lines() const {
  std::lock_guard lock(mutex_);
  return lines_;
}

void Logger::push(std::string level, std::string message) {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  localtime_r(&time, &tm);
  std::ostringstream stamp;
  stamp << std::put_time(&tm, "%F %T");
  std::lock_guard lock(mutex_);
  lines_.push_back(stamp.str() + " [" + level + "] " + message);
  if (lines_.size() > 1000) {
    lines_.erase(lines_.begin(), lines_.begin() + 100);
  }
}

}  // namespace boorubox
