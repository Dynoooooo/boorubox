#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "download/FilenameTemplate.hpp"
#include "http/HttpClient.hpp"
#include "index/ArchiveIndex.hpp"
#include "model/DownloadJob.hpp"
#include "util/ContentFilter.hpp"

namespace boorubox {

struct DownloadOptions {
  std::filesystem::path download_dir;
  std::string filename_template = "{provider}/{artist}/{id}_{md5}.{ext}";
  int max_concurrent_downloads = 3;
  int max_retries = 3;
  DownloadQuality quality = DownloadQuality::Original;
};

class DownloadManager {
 public:
  DownloadManager(HttpClient& http, ArchiveIndex& index, ContentRules rules,
                  std::shared_ptr<std::mutex> index_mutex,
                  DownloadOptions options);
  ~DownloadManager();

  std::size_t enqueue(Post post);
  void start();
  void stop();
  void retry_failed(std::size_t job_id);
  void cancel(std::size_t job_id);
  std::size_t clear_failed_and_skipped();
  void set_content_rules(ContentRules rules);
  std::vector<DownloadJob> jobs() const;

 private:
  void worker_loop(std::stop_token stop_token);
  void process_job(std::size_t job_id);
  std::string selected_url(const Post& post) const;
  bool is_cancelled(std::size_t job_id) const;
  bool has_duplicate_job_locked(std::size_t job_id) const;
  LocalArchiveItem archive_item_from_job(const DownloadJob& job) const;
  void update_job(std::size_t job_id, const std::function<void(DownloadJob&)>& fn);

  HttpClient& http_;
  ArchiveIndex& index_;
  std::shared_ptr<std::mutex> index_mutex_;
  ContentRules rules_;
  DownloadOptions options_;

  mutable std::mutex mutex_;
  std::condition_variable_any cv_;
  std::vector<DownloadJob> jobs_;
  std::vector<bool> hidden_jobs_;
  std::deque<std::size_t> pending_;
  std::vector<std::jthread> workers_;
};

}  // namespace boorubox
