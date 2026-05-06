#include <catch2/catch_test_macros.hpp>

#include "util/ContentFilter.hpp"

using namespace boorubox;

TEST_CASE("blacklist blocks matching tags case-insensitively") {
  Post post;
  post.rating = Rating::Safe;
  post.tags = {"Landscape", "Blocked_Tag"};
  ContentRules rules;
  rules.enable_nsfw = false;
  rules.blacklisted_tags = {"blocked_tag"};

  std::string reason;
  REQUIRE_FALSE(is_post_allowed(post, rules, &reason));
  REQUIRE(reason == "blocked by tag blacklist");
}

TEST_CASE("SFW mode blocks questionable and explicit ratings") {
  ContentRules rules;
  rules.enable_nsfw = false;

  Post questionable;
  questionable.rating = Rating::Questionable;
  REQUIRE_FALSE(is_post_allowed(questionable, rules));

  Post explicit_post;
  explicit_post.rating = Rating::Explicit;
  REQUIRE_FALSE(is_post_allowed(explicit_post, rules));

  Post safe;
  safe.rating = Rating::Safe;
  REQUIRE(is_post_allowed(safe, rules));
}
