#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "model/Rating.hpp"

namespace boorubox {

struct LocalArchiveItem {
  std::filesystem::path local_file_path;
  std::string provider;
  std::string post_id;
  std::string post_url;
  std::string file_url;
  std::filesystem::path thumbnail_path;
  std::filesystem::path preview_path;
  std::string hash;
  std::vector<std::string> tags;
  std::vector<std::string> artists;
  Rating rating = Rating::Unknown;
  int width = 0;
  int height = 0;
  std::string file_ext;
  std::int64_t file_size = 0;
  std::string downloaded_at;
  bool favorite = false;
  std::string notes;
};

}  // namespace boorubox
