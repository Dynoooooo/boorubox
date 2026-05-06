#pragma once

#include <string>

#include "providers/SiteProvider.hpp"

namespace boorubox {

class DanbooruProvider final : public SiteProvider {
 public:
  explicit DanbooruProvider(
      std::string base_url = "https://danbooru.donmai.us");

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
  std::string base_url_;
};

}  // namespace boorubox
