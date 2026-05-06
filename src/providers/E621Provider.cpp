#include "providers/E621Provider.hpp"

#include <array>
#include <stdexcept>

#include "providers/ProviderUtil.hpp"
#include "util/StringUtil.hpp"

namespace boorubox {

namespace {

std::string e621_rating_tag(Rating rating) {
  switch (rating) {
    case Rating::Safe:
    case Rating::General:
      return "rating:safe";
    case Rating::Questionable:
      return "rating:questionable";
    case Rating::Explicit:
      return "rating:explicit";
    case Rating::Sensitive:
    case Rating::Unknown:
      return {};
  }
  return {};
}

std::string nested_string(const nlohmann::json& object, std::string_view group,
                          std::string_view key) {
  const auto group_it = object.find(std::string(group));
  if (group_it == object.end() || !group_it->is_object()) {
    return {};
  }
  return providers::json_string(*group_it, key);
}

int nested_int(const nlohmann::json& object, std::string_view group,
               std::string_view key) {
  const auto group_it = object.find(std::string(group));
  if (group_it == object.end() || !group_it->is_object()) {
    return 0;
  }
  return providers::json_int(*group_it, key);
}

std::int64_t nested_int64(const nlohmann::json& object, std::string_view group,
                          std::string_view key) {
  const auto group_it = object.find(std::string(group));
  if (group_it == object.end() || !group_it->is_object()) {
    return 0;
  }
  return providers::json_int64(*group_it, key);
}

void append_tag_group(const nlohmann::json& tags, std::string_view key,
                      std::vector<std::string>& out) {
  const auto it = tags.find(std::string(key));
  if (it == tags.end() || !it->is_array()) {
    return;
  }
  for (const auto& tag : *it) {
    if (tag.is_string()) {
      out.push_back(tag.get<std::string>());
    }
  }
}

}  // namespace

E621Provider::E621Provider(std::string provider_name, std::string base_url,
                           bool nsfw_provider)
    : provider_name_(std::move(provider_name)),
      base_url_(providers::without_trailing_slash(std::move(base_url))),
      nsfw_provider_(nsfw_provider) {}

std::string E621Provider::name() const {
  return provider_name_;
}

std::string E621Provider::base_url() const {
  return base_url_;
}

ProviderCapabilities E621Provider::capabilities() const {
  return ProviderCapabilities{
      .supports_json = true,
      .supports_rating_filter = true,
      .supports_random = false,
      .supports_auth = true,
      .max_limit = 320,
  };
}

bool E621Provider::is_nsfw_provider() const {
  return nsfw_provider_;
}

bool E621Provider::requires_auth() const {
  return false;
}

RateLimitPolicy E621Provider::rate_limit_policy() const {
  return RateLimitPolicy{.delay = std::chrono::milliseconds(1000)};
}

HttpRequest E621Provider::build_search_request(const SearchQuery& query,
                                               const SearchSafety& safety) const {
  auto terms = providers::tag_terms_with_exclusions(query, safety);
  if (!safety.enable_nsfw) {
    terms.push_back("rating:safe");
  } else if (const auto rating = e621_rating_tag(query.rating_filter);
             !rating.empty()) {
    terms.push_back(rating);
  }
  if (!query.order.empty()) {
    terms.push_back("order:" + query.order);
  }

  const std::vector<std::pair<std::string, std::string>> params = {
      {"tags", providers::joined_tag_terms(terms)},
      {"limit", std::to_string(providers::clamp_limit(query.limit, 320))},
      {"page", std::to_string(query.page <= 0 ? 1 : query.page)},
  };
  return HttpRequest{
      .url = providers::append_query_params(base_url_ + "/posts.json", params),
      .headers = {},
  };
}

std::vector<Post> E621Provider::parse_search_response(
    std::string_view response_body) const {
  const auto root = nlohmann::json::parse(response_body);
  if (!root.is_object() || !root.contains("posts") || !root["posts"].is_array()) {
    throw std::runtime_error("e621/e926 posts response missing posts array");
  }
  std::vector<Post> posts;
  posts.reserve(root["posts"].size());
  for (const auto& raw : root["posts"]) {
    posts.push_back(normalize_post(raw));
  }
  return posts;
}

Post E621Provider::normalize_post(const nlohmann::json& raw_post) const {
  Post post;
  post.provider = name();
  post.id = providers::json_string(raw_post, "id");
  post.post_url = base_url_ + "/posts/" + post.id;
  post.file_url =
      providers::absolutize_url(base_url_, nested_string(raw_post, "file", "url"));
  post.sample_url =
      providers::absolutize_url(base_url_, nested_string(raw_post, "sample", "url"));
  post.preview_url =
      providers::absolutize_url(base_url_, nested_string(raw_post, "preview", "url"));
  post.thumbnail_url = post.preview_url;
  post.hash = nested_string(raw_post, "file", "md5");
  post.width = nested_int(raw_post, "file", "width");
  post.height = nested_int(raw_post, "file", "height");
  post.file_ext = nested_string(raw_post, "file", "ext");
  post.file_size = nested_int64(raw_post, "file", "size");
  post.rating = rating_from_string(providers::json_string(raw_post, "rating"));
  post.score = nested_int(raw_post, "score", "total");
  post.favorites = providers::json_int(raw_post, "fav_count");
  post.created_at = providers::json_string(raw_post, "created_at");

  if (const auto tags_it = raw_post.find("tags");
      tags_it != raw_post.end() && tags_it->is_object()) {
    for (const auto key : std::array<std::string_view, 8>{
             "general", "species", "character", "copyright",
             "artist",  "lore",    "meta",      "invalid"}) {
      append_tag_group(*tags_it, key, post.tags);
    }
    append_tag_group(*tags_it, "artist", post.artist_tags);
  }

  if (const auto sources_it = raw_post.find("sources");
      sources_it != raw_post.end() && sources_it->is_array()) {
    std::vector<std::string> sources;
    for (const auto& source : *sources_it) {
      if (source.is_string()) {
        sources.push_back(source.get<std::string>());
      }
    }
    post.source = join(sources, "\n");
  }

  return post;
}

}  // namespace boorubox
