#include "providers/GelbooruProvider.hpp"

#include <stdexcept>

#include "providers/ProviderUtil.hpp"
#include "util/PathUtil.hpp"
#include "util/StringUtil.hpp"

namespace boorubox {

namespace {

std::string gelbooru_rating_tag(Rating rating) {
  switch (rating) {
    case Rating::Safe:
    case Rating::General:
      return "rating:general";
    case Rating::Sensitive:
      return "rating:sensitive";
    case Rating::Questionable:
      return "rating:questionable";
    case Rating::Explicit:
      return "rating:explicit";
    case Rating::Unknown:
      return {};
  }
  return {};
}

void add_gelbooru_posts(const nlohmann::json& raw, std::vector<Post>& posts,
                        const GelbooruProvider& provider) {
  if (raw.is_array()) {
    for (const auto& item : raw) {
      posts.push_back(provider.normalize_post(item));
    }
    return;
  }
  if (raw.is_object()) {
    if (const auto it = raw.find("post"); it != raw.end()) {
      if (it->is_array()) {
        for (const auto& item : *it) {
          posts.push_back(provider.normalize_post(item));
        }
      } else if (it->is_object()) {
        posts.push_back(provider.normalize_post(*it));
      }
    }
  }
}

}  // namespace

GelbooruProvider::GelbooruProvider(std::string provider_name,
                                   std::string base_url, bool nsfw_provider,
                                   int max_limit, std::string user_id,
                                   std::string api_key)
    : provider_name_(std::move(provider_name)),
      base_url_(providers::without_trailing_slash(std::move(base_url))),
      nsfw_provider_(nsfw_provider),
      max_limit_(max_limit),
      user_id_(std::move(user_id)),
      api_key_(std::move(api_key)) {}

std::string GelbooruProvider::name() const {
  return provider_name_;
}

std::string GelbooruProvider::base_url() const {
  return base_url_;
}

ProviderCapabilities GelbooruProvider::capabilities() const {
  return ProviderCapabilities{
      .supports_json = true,
      .supports_rating_filter = true,
      .supports_random = false,
      .supports_auth = true,
      .max_limit = max_limit_,
  };
}

bool GelbooruProvider::is_nsfw_provider() const {
  return nsfw_provider_;
}

bool GelbooruProvider::requires_auth() const {
  return false;
}

RateLimitPolicy GelbooruProvider::rate_limit_policy() const {
  return RateLimitPolicy{.delay = std::chrono::milliseconds(1000)};
}

HttpRequest GelbooruProvider::build_search_request(
    const SearchQuery& query, const SearchSafety& safety) const {
  auto terms = providers::tag_terms_with_exclusions(query, safety);
  if (!safety.enable_nsfw) {
    terms.push_back("rating:general");
  } else if (const auto rating = gelbooru_rating_tag(query.rating_filter);
             !rating.empty()) {
    terms.push_back(rating);
  }
  if (!query.order.empty()) {
    terms.push_back("sort:" + query.order);
  }

  const int pid = query.page <= 1 ? 0 : query.page - 1;
  std::vector<std::pair<std::string, std::string>> params = {
      {"page", "dapi"},
      {"s", "post"},
      {"q", "index"},
      {"json", "1"},
      {"limit", std::to_string(providers::clamp_limit(query.limit, max_limit_))},
      {"pid", std::to_string(pid)},
      {"tags", providers::joined_tag_terms(terms)},
  };
  if (!user_id_.empty()) {
    params.emplace_back("user_id", user_id_);
  }
  if (!api_key_.empty()) {
    params.emplace_back("api_key", api_key_);
  }
  return HttpRequest{
      .url = providers::append_query_params(base_url_ + "/index.php", params),
      .headers = {},
  };
}

std::vector<Post> GelbooruProvider::parse_search_response(
    std::string_view response_body) const {
  if (trim(response_body).empty()) {
    return {};
  }
  const auto root = nlohmann::json::parse(response_body);
  std::vector<Post> posts;
  add_gelbooru_posts(root, posts, *this);
  return posts;
}

Post GelbooruProvider::normalize_post(const nlohmann::json& raw_post) const {
  Post post;
  post.provider = name();
  post.id = providers::json_string(raw_post, "id");
  post.post_url = base_url_ + "/index.php?page=post&s=view&id=" + post.id;
  post.file_url =
      providers::absolutize_url(base_url_, providers::json_string(raw_post, "file_url"));
  post.sample_url = providers::absolutize_url(
      base_url_, providers::json_string(raw_post, "sample_url"));
  post.preview_url = providers::absolutize_url(
      base_url_, providers::json_string(raw_post, "preview_url"));
  post.thumbnail_url = post.preview_url;
  post.hash = providers::json_string(raw_post, "md5");
  if (post.hash.empty()) {
    post.hash = providers::json_string(raw_post, "hash");
  }
  post.width = providers::json_int(raw_post, "width");
  if (post.width == 0) {
    post.width = providers::json_int(raw_post, "image_width");
  }
  post.height = providers::json_int(raw_post, "height");
  if (post.height == 0) {
    post.height = providers::json_int(raw_post, "image_height");
  }
  post.file_ext = providers::json_string(raw_post, "file_ext");
  if (post.file_ext.empty()) {
    post.file_ext = extension_from_url(post.file_url);
  }
  post.file_size = providers::json_int64(raw_post, "file_size");
  post.rating = rating_from_string(providers::json_string(raw_post, "rating"));
  post.score = providers::json_int(raw_post, "score");
  post.tags = providers::split_tag_string(providers::json_string(raw_post, "tags"));
  post.source = providers::json_string(raw_post, "source");
  post.created_at = providers::json_string(raw_post, "created_at");
  return post;
}

}  // namespace boorubox
