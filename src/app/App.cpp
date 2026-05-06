#include "app/App.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <stdexcept>

#include "index/JsonlArchiveIndex.hpp"
#include "index/SqliteArchiveIndex.hpp"
#include "providers/DanbooruProvider.hpp"
#include "providers/E621Provider.hpp"
#include "providers/GelbooruProvider.hpp"
#include "providers/SafebooruProvider.hpp"
#include "util/ContentFilter.hpp"
#include "util/PathUtil.hpp"
#include "util/StringUtil.hpp"

namespace boorubox {

namespace {

std::unique_ptr<ArchiveIndex> make_archive_index(const std::filesystem::path& path) {
  const auto ext = lower(path.extension().string());
  if (ext == ".sqlite" || ext == ".sqlite3" || ext == ".db") {
    return std::make_unique<SqliteArchiveIndex>(path);
  }
  return std::make_unique<JsonlArchiveIndex>(path);
}

DownloadQuality download_quality_from_config(std::string value) {
  value = lower(value);
  if (value == "sample") {
    return DownloadQuality::Sample;
  }
  if (value == "preview") {
    return DownloadQuality::Preview;
  }
  return DownloadQuality::Original;
}

bool matches_requested_rating(Rating actual, Rating requested) {
  if (requested == Rating::Unknown) {
    return true;
  }
  if (requested == Rating::Safe || requested == Rating::General) {
    return actual == Rating::Safe || actual == Rating::General;
  }
  return actual == requested;
}

bool can_expand_tag(std::string_view tag) {
  if (tag.empty() || tag.starts_with("-") || tag.find(':') != std::string::npos ||
      tag.find('*') != std::string::npos || tag.find('?') != std::string::npos) {
    return false;
  }
  return std::ranges::all_of(tag, [](unsigned char ch) {
    return std::isalnum(ch) || ch == '_' || ch == '-';
  });
}

std::optional<SearchQuery> wildcard_retry_query(const SearchQuery& query) {
  if (query.tags.empty()) {
    return std::nullopt;
  }
  SearchQuery retry = query;
  bool changed = false;
  for (auto& tag : retry.tags) {
    if (!can_expand_tag(tag)) {
      return std::nullopt;
    }
    tag += '*';
    changed = true;
  }
  if (!changed) {
    return std::nullopt;
  }
  return retry;
}

}  // namespace

App::App(AppConfig config)
    : config_(std::move(config)),
      http_(config_.user_agent),
      index_(make_archive_index(config_.index_path)),
      preview_cache_(config_.cache_dir, http_),
      download_manager_(
          http_, *index_, content_rules(),
          DownloadOptions{
              .download_dir = config_.download_dir,
              .filename_template = config_.filename_template,
              .max_concurrent_downloads = config_.max_concurrent_downloads,
              .max_retries = 3,
              .quality = download_quality_from_config(
                  config_.preferred_download_quality),
          }) {
  ensure_directory(config_.download_dir);
  ensure_directory(config_.cache_dir);
  ensure_directory(config_.index_path.parent_path());
  register_providers();
}

const AppConfig& App::config() const {
  return config_;
}

void App::apply_runtime_config(AppConfig config) {
  config_ = std::move(config);
  rebuild_providers();
  download_manager_.set_content_rules(content_rules());
}

void App::set_enable_nsfw(bool enabled) {
  config_.enable_nsfw = enabled;
  download_manager_.set_content_rules(content_rules());
}

std::vector<std::string> App::provider_names() const {
  std::vector<std::string> names;
  for (const auto& provider : providers_) {
    if (!config_.enable_nsfw && provider->is_nsfw_provider()) {
      continue;
    }
    names.push_back(provider->name());
  }
  return names;
}

std::vector<Post> App::search(const SearchQuery& query) {
  auto* provider = provider_by_name(query.provider_name);
  if (provider == nullptr) {
    throw std::runtime_error("unknown provider: " + query.provider_name);
  }
  if (!config_.enable_nsfw && provider->is_nsfw_provider()) {
    throw std::runtime_error("provider is hidden while enable_nsfw=false: " +
                             query.provider_name);
  }

  const auto run_query = [&](const SearchQuery& current_query,
                             bool retry) -> std::pair<std::vector<Post>, std::size_t> {
    const auto request =
        provider->build_search_request(current_query, search_safety());
    logger_.info(std::string(retry ? "Search retry on " : "Search started on ") +
                 current_query.provider_name + ": tags=\"" +
                 join(current_query.tags, " ") + "\" excluded=\"" +
                 join(current_query.excluded_tags, " ") + "\" rating=" +
                 to_string(current_query.rating_filter) +
                 (config_.enable_nsfw ? " nsfw=on" : " sfw=on"));
    logger_.info("GET " + request.url);
    for (std::size_t i = 0; i < providers_.size(); ++i) {
      if (providers_[i].get() == provider) {
        rate_limiters_[i]->wait();
        break;
      }
    }

    const auto response = http_.get(request.url, request.headers);
    auto posts = provider->parse_search_response(response.body);
    const auto raw_count = posts.size();
    const auto before_rating = posts.size();
    std::erase_if(posts, [&](const Post& post) {
      return !matches_requested_rating(post.rating, current_query.rating_filter);
    });
    if (posts.size() != before_rating) {
      logger_.info("filtered " + std::to_string(before_rating - posts.size()) +
                   " result(s) by requested rating");
    }
    const auto before_safety = posts.size();
    posts = filter_posts(std::move(posts), content_rules());
    if (posts.size() != before_safety) {
      logger_.warn("filtered " + std::to_string(before_safety - posts.size()) +
                   " result(s) by safety rules");
    }
    return {std::move(posts), raw_count};
  };

  auto [posts, raw_count] = run_query(query, false);
  if (posts.empty()) {
    if (const auto retry = wildcard_retry_query(query)) {
      logger_.info("No results for exact tags on " + query.provider_name +
                   "; retrying with prefix wildcard tags");
      auto [retry_posts, retry_raw_count] = run_query(*retry, true);
      if (!retry_posts.empty() || retry_raw_count > 0) {
        posts = std::move(retry_posts);
        raw_count = retry_raw_count;
      }
    }
  }

  logger_.info("Search completed on " + query.provider_name + ": " +
               std::to_string(posts.size()) + " result(s) shown from " +
               std::to_string(raw_count) + " raw result(s)");
  if (posts.empty()) {
    logger_.warn("Zero results on " + query.provider_name +
                 ". Try the provider's exact tag name, for example a "
                 "disambiguated character tag like name_(series).");
  }
  return posts;
}

std::size_t App::enqueue_download(const Post& post) {
  std::string reason;
  if (!is_post_allowed(post, content_rules(), &reason)) {
    logger_.warn("refused download for post " + post.provider + "/" + post.id +
                 ": " + reason);
    throw std::runtime_error(reason);
  }
  return download_manager_.enqueue(post);
}

void App::start_downloads() {
  download_manager_.start();
}

void App::retry_download(std::size_t job_id) {
  download_manager_.retry_failed(job_id);
}

void App::cancel_download(std::size_t job_id) {
  download_manager_.cancel(job_id);
}

std::size_t App::clear_failed_and_skipped_downloads() {
  const auto cleared = download_manager_.clear_failed_and_skipped();
  if (cleared > 0) {
    logger_.info("Cleared " + std::to_string(cleared) +
                 " failed/skipped download job(s)");
  }
  return cleared;
}

std::vector<DownloadJob> App::download_jobs() const {
  return download_manager_.jobs();
}

std::vector<LocalArchiveItem> App::archive_items(const ArchiveQuery& query) const {
  return index_->list(query);
}

void App::rebuild_archive() {
  index_->rebuild_from_directory(config_.download_dir);
}

std::filesystem::path App::ensure_preview(const Post& post) {
  return preview_cache_.ensure_preview(post);
}

std::vector<std::string> App::logs() const {
  return logger_.lines();
}

void App::log_info(std::string message) {
  logger_.info(std::move(message));
}

void App::log_warn(std::string message) {
  logger_.warn(std::move(message));
}

void App::log_error(std::string message) {
  logger_.error(std::move(message));
}

SiteProvider* App::provider_by_name(const std::string& name) const {
  for (const auto& provider : providers_) {
    if (provider->name() == name) {
      return provider.get();
    }
  }
  return nullptr;
}

SearchSafety App::search_safety() const {
  return SearchSafety{
      .enable_nsfw = config_.enable_nsfw,
      .default_rating = config_.default_rating,
      .blacklisted_tags = config_.blacklisted_tags,
  };
}

ContentRules App::content_rules() const {
  return ContentRules{
      .enable_nsfw = config_.enable_nsfw,
      .blacklisted_tags = config_.blacklisted_tags,
  };
}

void App::register_providers() {
  const auto add_rate_limiter = [&](const SiteProvider& provider) {
    const auto policy = provider.rate_limit_policy();
    rate_limiters_.push_back(std::make_unique<RateLimiter>(policy.delay));
  };

  const auto add_provider = [&](std::unique_ptr<SiteProvider> provider) {
    add_rate_limiter(*provider);
    providers_.push_back(std::move(provider));
  };

  if (const auto it = config_.providers.find("danbooru");
      it != config_.providers.end() && it->second.enabled) {
    add_provider(std::make_unique<DanbooruProvider>(it->second.base_url));
  }
  if (const auto it = config_.providers.find("safebooru");
      it != config_.providers.end() && it->second.enabled) {
    add_provider(std::make_unique<SafebooruProvider>(it->second.base_url));
  }
  if (const auto it = config_.providers.find("gelbooru");
      it != config_.providers.end() && it->second.enabled) {
    add_provider(std::make_unique<GelbooruProvider>(
        "gelbooru", it->second.base_url, it->second.nsfw_provider, 100,
        it->second.login, it->second.api_key));
  }
  if (const auto it = config_.providers.find("e926");
      it != config_.providers.end() && it->second.enabled) {
    add_provider(
        std::make_unique<E621Provider>("e926", it->second.base_url, false));
  }
  if (const auto it = config_.providers.find("e621");
      it != config_.providers.end() && it->second.enabled) {
    add_provider(std::make_unique<E621Provider>(
        "e621", it->second.base_url, it->second.nsfw_provider));
  }
}

void App::rebuild_providers() {
  providers_.clear();
  rate_limiters_.clear();
  register_providers();
}

}  // namespace boorubox
