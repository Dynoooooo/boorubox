#pragma once

#include <string>

#include "providers/SiteProvider.hpp"

namespace boorubox {

class GelbooruProvider : public SiteProvider {
 public:
  GelbooruProvider(std::string provider_name = "gelbooru",
                   std::string base_url = "https://gelbooru.com",
                   bool nsfw_provider = false, int max_limit = 100,
                   std::string user_id = {}, std::string api_key = {});

  std::string name() const override;
  std::string base_url() const override;
  ProviderCapabilities capabilities() const override;
  bool is_nsfw_provider() const override;
  bool requires_auth() const override;
  RateLimitPolicy rate_limit_policy() const override;
  HttpRequest build_search_request(const SearchQuery& query,
                                   const SearchSafety& safety) const override;
  std::vector<Post> parse_search_response(
      std::string_view response_body) const override;
  Post normalize_post(const nlohmann::json& raw_post) const override;

 private:
  std::string provider_name_;
  std::string base_url_;
  bool nsfw_provider_ = false;
  int max_limit_ = 100;
  std::string user_id_;
  std::string api_key_;
};

}  // namespace boorubox
