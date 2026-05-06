#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace boorubox {

class Logger {
 public:
  void info(std::string message);
  void warn(std::string message);
  void error(std::string message);
  std::vector<std::string> lines() const;

 private:
  void push(std::string level, std::string message);

  mutable std::mutex mutex_;
  std::vector<std::string> lines_;
};

}  // namespace boorubox
