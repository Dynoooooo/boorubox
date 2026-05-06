#include "util/ContentFilter.hpp"

#include <unordered_set>

#include "model/Rating.hpp"
#include "util/StringUtil.hpp"

namespace boorubox {

bool has_blacklisted_tag(const Post& post,
                         const std::vector<std::string>& blacklisted_tags) {
  std::unordered_set<std::string> blacklist;
  for (const auto& tag : blacklisted_tags) {
    blacklist.insert(lower(tag));
  }

  for (const auto& tag : post.tags) {
    if (blacklist.contains(lower(tag))) {
      return true;
    }
  }
  for (const auto& tag : post.artist_tags) {
    if (blacklist.contains(lower(tag))) {
      return true;
    }
  }
  return false;
}

bool is_post_allowed(const Post& post, const ContentRules& rules,
                     std::string* reason) {
  if (!rules.enable_nsfw && !is_allowed_in_sfw_mode(post.rating)) {
    if (reason != nullptr) {
      *reason = "blocked by SFW rating policy";
    }
    return false;
  }
  if (has_blacklisted_tag(post, rules.blacklisted_tags)) {
    if (reason != nullptr) {
      *reason = "blocked by tag blacklist";
    }
    return false;
  }
  return true;
}

std::vector<Post> filter_posts(std::vector<Post> posts,
                               const ContentRules& rules) {
  std::vector<Post> out;
  out.reserve(posts.size());
  for (auto& post : posts) {
    if (is_post_allowed(post, rules)) {
      out.push_back(std::move(post));
    }
  }
  return out;
}

}  // namespace boorubox
