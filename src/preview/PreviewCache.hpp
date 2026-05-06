#pragma once

#include <filesystem>

#include "http/HttpClient.hpp"
#include "model/Post.hpp"

namespace boorubox {

class PreviewCache {
 public:
  PreviewCache(std::filesystem::path cache_dir, HttpClient& http);

  std::filesystem::path cached_path_for(const Post& post) const;
  std::filesystem::path ensure_preview(const Post& post);

 private:
  std::filesystem::path cache_dir_;
  HttpClient& http_;
};

}  // namespace boorubox
