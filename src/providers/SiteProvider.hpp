#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "model/Post.hpp"
#include "model/SearchQuery.hpp"

namespace boorubox {

struct ProviderCapabilities {
  bool supports_json = true;
  bool supports_rating_filter = true;
  bool supports_random = false;
  bool supports_auth = false;
  int max_limit = 100;
};

struct RateLimitPolicy {
  std::chrono::milliseconds delay{1000};
};

struct SearchSafety {
  bool enable_nsfw = false;
  Rating default_rating = Rating::Safe;
  std::vector<std::string> blacklisted_tags;
};

struct HttpRequest {
  std::string url;
  std::vector<std::string> headers;
};

class SiteProvider {
 public:
  virtual ~SiteProvider() = default;

  virtual std::string name() const = 0;
  virtual std::string base_url() const = 0;
  virtual ProviderCapabilities capabilities() const = 0;
  virtual bool is_nsfw_provider() const = 0;
  virtual bool requires_auth() const = 0;
  virtual RateLimitPolicy rate_limit_policy() const = 0;

  virtual HttpRequest build_search_request(
      const SearchQuery& query, const SearchSafety& safety) const = 0;
  virtual std::vector<Post> parse_search_response(
      std::string_view response_body) const = 0;
  virtual Post normalize_post(const nlohmann::json& raw_post) const = 0;
};

}  // namespace boorubox
