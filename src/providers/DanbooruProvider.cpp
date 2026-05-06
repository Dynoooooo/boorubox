#include "providers/DanbooruProvider.hpp"

#include <stdexcept>

#include "providers/ProviderUtil.hpp"

namespace boorubox {

namespace {

std::string danbooru_rating_tag(Rating rating) {
  switch (rating) {
    case Rating::Safe:
    case Rating::General:
      return "rating:g";
    case Rating::Sensitive:
      return "rating:s";
    case Rating::Questionable:
      return "rating:q";
    case Rating::Explicit:
      return "rating:e";
    case Rating::Unknown:
      return {};
  }
  return {};
}

}  // namespace

DanbooruProvider::DanbooruProvider(std::string base_url)
    : base_url_(providers::without_trailing_slash(std::move(base_url))) {}

std::string DanbooruProvider::name() const {
  return "danbooru";
}

std::string DanbooruProvider::base_url() const {
  return base_url_;
}

ProviderCapabilities DanbooruProvider::capabilities() const {
  return ProviderCapabilities{
      .supports_json = true,
      .supports_rating_filter = true,
      .supports_random = true,
      .supports_auth = true,
      .max_limit = 200,
  };
}

bool DanbooruProvider::is_nsfw_provider() const {
  return false;
}

bool DanbooruProvider::requires_auth() const {
  return false;
}

RateLimitPolicy DanbooruProvider::rate_limit_policy() const {
  return RateLimitPolicy{.delay = std::chrono::milliseconds(1000)};
}

HttpRequest DanbooruProvider::build_search_request(
    const SearchQuery& query, const SearchSafety& safety) const {
  std::vector<std::string> terms;
  const auto add_if_room = [&](const std::string& term) {
    if (!term.empty() && terms.size() < 2) {
      terms.push_back(term);
    }
  };

  for (const auto& tag : query.tags) {
    add_if_room(tag);
  }

  if (!safety.enable_nsfw) {
    add_if_room("rating:g");
  } else if (const auto rating = danbooru_rating_tag(query.rating_filter);
             !rating.empty()) {
    add_if_room(rating);
  }

  for (const auto& tag : query.excluded_tags) {
    add_if_room("-" + tag);
  }
  for (const auto& tag : safety.blacklisted_tags) {
    add_if_room("-" + tag);
  }

  if (!query.order.empty()) {
    add_if_room("order:" + query.order);
  }

  std::vector<std::pair<std::string, std::string>> params = {
      {"tags", providers::joined_tag_terms(terms)},
      {"limit", std::to_string(providers::clamp_limit(query.limit, 200))},
      {"page", std::to_string(query.page <= 0 ? 1 : query.page)},
  };
  if (query.random_mode) {
    params.emplace_back("random", "true");
  }

  return HttpRequest{
      .url = providers::append_query_params(base_url_ + "/posts.json", params),
      .headers = {},
  };
}

std::vector<Post> DanbooruProvider::parse_search_response(
    std::string_view response_body) const {
  const auto root = nlohmann::json::parse(response_body);
  if (!root.is_array()) {
    throw std::runtime_error("Danbooru posts response was not a JSON array");
  }

  std::vector<Post> posts;
  posts.reserve(root.size());
  for (const auto& raw : root) {
    posts.push_back(normalize_post(raw));
  }
  return posts;
}

Post DanbooruProvider::normalize_post(const nlohmann::json& raw_post) const {
  Post post;
  post.provider = name();
  post.id = providers::json_string(raw_post, "id");
  post.post_url = base_url_ + "/posts/" + post.id;
  post.file_url =
      providers::absolutize_url(base_url_, providers::json_string(raw_post, "file_url"));
  post.sample_url = providers::absolutize_url(
      base_url_, providers::json_string(raw_post, "large_file_url"));
  post.preview_url = providers::absolutize_url(
      base_url_, providers::json_string(raw_post, "preview_file_url"));
  post.thumbnail_url = post.preview_url;
  post.hash = providers::json_string(raw_post, "md5");
  post.width = providers::json_int(raw_post, "image_width");
  post.height = providers::json_int(raw_post, "image_height");
  post.file_ext = providers::json_string(raw_post, "file_ext");
  post.file_size = providers::json_int64(raw_post, "file_size");
  post.rating = rating_from_string(providers::json_string(raw_post, "rating"));
  post.score = providers::json_int(raw_post, "score");
  post.favorites = providers::json_int(raw_post, "fav_count");
  post.tags =
      providers::split_tag_string(providers::json_string(raw_post, "tag_string"));
  post.artist_tags = providers::split_tag_string(
      providers::json_string(raw_post, "tag_string_artist"));
  post.source = providers::json_string(raw_post, "source");
  post.created_at = providers::json_string(raw_post, "created_at");
  return post;
}

}  // namespace boorubox
