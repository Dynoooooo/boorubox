#include "preview/PreviewCache.hpp"

#include "util/PathUtil.hpp"
#include "util/StringUtil.hpp"

namespace boorubox {

PreviewCache::PreviewCache(std::filesystem::path cache_dir, HttpClient& http)
    : cache_dir_(std::move(cache_dir)), http_(http) {}

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
  http_.download_to_file(url, path, false);
  return path;
}

}  // namespace boorubox
