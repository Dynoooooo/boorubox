#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "app/Config.hpp"
#include "download/DownloadManager.hpp"
#include "http/HttpClient.hpp"
#include "index/ArchiveIndex.hpp"
#include "preview/PreviewCache.hpp"
#include "providers/SiteProvider.hpp"
#include "util/Logger.hpp"
#include "util/RateLimiter.hpp"

namespace boorubox {

class App {
 public:
  explicit App(AppConfig config);

  AppConfig config() const;
  void apply_runtime_config(AppConfig config);
  void set_enable_nsfw(bool enabled);
  std::vector<std::string> provider_names() const;
  std::vector<Post> search(const SearchQuery& query);
  std::size_t enqueue_download(const Post& post);
  void start_downloads();
  void retry_download(std::size_t job_id);
  void cancel_download(std::size_t job_id);
  std::size_t clear_failed_and_skipped_downloads();
  std::vector<DownloadJob> download_jobs() const;
  std::vector<LocalArchiveItem> archive_items(const ArchiveQuery& query = {}) const;
  void rebuild_archive();
  std::filesystem::path ensure_preview(const Post& post);
  std::vector<std::string> logs() const;
  void log_info(std::string message);
  void log_warn(std::string message);
  void log_error(std::string message);

 private:
  SiteProvider* provider_by_name(const std::string& name) const;
  SearchSafety search_safety_unlocked() const;
  ContentRules content_rules_unlocked() const;
  void register_providers();
  void rebuild_providers();

  mutable std::mutex state_mutex_;
  AppConfig config_;
  HttpClient http_;
  std::shared_ptr<std::mutex> archive_mutex_;
  std::unique_ptr<ArchiveIndex> index_;
  PreviewCache preview_cache_;
  DownloadManager download_manager_;
  Logger logger_;
  std::vector<std::unique_ptr<SiteProvider>> providers_;
  std::vector<std::unique_ptr<RateLimiter>> rate_limiters_;
};

}  // namespace boorubox
