#pragma once

#include <string>
#include <vector>

#include "model/Rating.hpp"

namespace boorubox {

struct SearchQuery {
  std::string provider_name;
  std::vector<std::string> tags;
  std::vector<std::string> excluded_tags;
  Rating rating_filter = Rating::Safe;
  int page = 1;
  int limit = 20;
  std::string order;
  bool random_mode = false;
};

}  // namespace boorubox
