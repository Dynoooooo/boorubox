#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "model/Rating.hpp"

namespace boorubox {

struct Post {
  std::string id;
  std::string provider;
  std::string post_url;
  std::string file_url;
  std::string sample_url;
  std::string preview_url;
  std::string thumbnail_url;
  std::string hash;
  int width = 0;
  int height = 0;
  std::string file_ext;
  std::int64_t file_size = 0;
  Rating rating = Rating::Unknown;
  int score = 0;
  int favorites = 0;
  std::vector<std::string> tags;
  std::vector<std::string> artist_tags;
  std::string source;
  std::string created_at;
};

}  // namespace boorubox
