#pragma once

#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <unordered_set>

#include "http/HttpClient.hpp"
#include "model/Post.hpp"
#include "util/RateLimiter.hpp"

namespace boorubox {

class PreviewCache {
 public:
  PreviewCache(std::filesystem::path cache_dir, HttpClient& http);

  std::filesystem::path cached_path_for(const Post& post) const;
  std::filesystem::path ensure_preview(const Post& post);

 private:
  std::filesystem::path cache_dir_;
  HttpClient& http_;
  RateLimiter rate_limiter_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::unordered_set<std::string> in_flight_;
};

}  // namespace boorubox
