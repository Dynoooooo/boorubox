#include "preview/PreviewCache.hpp"

#include <chrono>

#include "util/PathUtil.hpp"
#include "util/StringUtil.hpp"

namespace boorubox {

PreviewCache::PreviewCache(std::filesystem::path cache_dir, HttpClient& http)
    : cache_dir_(std::move(cache_dir)),
      http_(http),
      rate_limiter_(std::chrono::milliseconds(250)) {}

std::filesystem::path PreviewCache::cached_path_for(const Post& post) const {
  auto ext = extension_from_url(post.preview_url);
  if (ext.empty()) {
    ext = extension_from_url(post.sample_url);
  }
  if (ext.empty()) {
    ext = "img";
  }
  const auto key = sanitize_filename_component(
      post.provider + "_" + (post.hash.empty() ? post.id : post.hash));
  return cache_dir_ / "previews" / post.provider / (key + "." + ext);
}

std::filesystem::path PreviewCache::ensure_preview(const Post& post) {
  const auto path = cached_path_for(post);
  if (std::filesystem::exists(path)) {
    return path;
  }
  const auto url = !post.preview_url.empty()
                       ? post.preview_url
                       : (!post.sample_url.empty() ? post.sample_url : post.file_url);
  if (url.empty()) {
    return {};
  }

  const auto key = path.string();
  {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [&] { return !in_flight_.contains(key); });
    if (std::filesystem::exists(path)) {
      return path;
    }
    in_flight_.insert(key);
  }

  struct InFlightGuard {
    PreviewCache& cache;
    std::string key;

    ~InFlightGuard() {
      {
        std::lock_guard lock(cache.mutex_);
        cache.in_flight_.erase(key);
      }
      cache.cv_.notify_all();
    }
  } guard{*this, key};

  const auto temp_path = path.string() + ".part";
  try {
    rate_limiter_.wait();
    http_.download_to_file(url, temp_path, false);
    std::filesystem::rename(temp_path, path);
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(temp_path, ignored);
    throw;
  }
  return path;
}

}  // namespace boorubox
