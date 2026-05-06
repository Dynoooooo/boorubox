#pragma once

#include "providers/GelbooruProvider.hpp"

namespace boorubox {

class SafebooruProvider final : public GelbooruProvider {
 public:
  explicit SafebooruProvider(std::string base_url = "https://safebooru.org");
};

}  // namespace boorubox
