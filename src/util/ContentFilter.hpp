#pragma once

#include <string>
#include <vector>

#include "model/Post.hpp"

namespace boorubox {

struct ContentRules {
  bool enable_nsfw = false;
  std::vector<std::string> blacklisted_tags;
};

bool has_blacklisted_tag(const Post& post,
                         const std::vector<std::string>& blacklisted_tags);
bool is_post_allowed(const Post& post, const ContentRules& rules,
                     std::string* reason = nullptr);
std::vector<Post> filter_posts(std::vector<Post> posts,
                               const ContentRules& rules);

}  // namespace boorubox
