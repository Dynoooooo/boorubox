#include "download/DownloadManager.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "util/PathUtil.hpp"

namespace boorubox {

namespace {

std::string now_iso8601() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&time, &tm);
  std::ostringstream out;
  out << std::put_time(&tm, "%FT%TZ");
  return out.str();
}

bool is_terminal_status(DownloadStatus status) {
  return status == DownloadStatus::Complete || status == DownloadStatus::Skipped ||
         status == DownloadStatus::Failed || status == DownloadStatus::Cancelled;
}

}  // namespace

DownloadManager::DownloadManager(HttpClient& http, ArchiveIndex& index,
                                 ContentRules rules, DownloadOptions options)
    : http_(http),
      index_(index),
      rules_(std::move(rules)),
      options_(std::move(options)) {}

DownloadManager::~DownloadManager() {
  stop();
}

std::size_t DownloadManager::enqueue(Post post) {
  FilenameTemplate filename_template(options_.filename_template);
  DownloadJob job;
  job.post = std::move(post);
  job.destination_path =
      filename_template.render_under(options_.download_dir, job.post);
  job.temp_path = job.destination_path.string() + ".part";

  std::lock_guard lock(mutex_);
  job.id = jobs_.size();
  const auto id = job.id;
  jobs_.push_back(std::move(job));
  hidden_jobs_.push_back(false);
  pending_.push_back(id);
  cv_.notify_one();
  return id;
}

void DownloadManager::start() {
  std::lock_guard lock(mutex_);
  if (!workers_.empty()) {
    return;
  }
  const int worker_count = std::max(1, options_.max_concurrent_downloads);
  for (int i = 0; i < worker_count; ++i) {
    workers_.emplace_back([this](std::stop_token token) { worker_loop(token); });
  }
}

void DownloadManager::stop() {
  {
    std::lock_guard lock(mutex_);
    for (auto& worker : workers_) {
      worker.request_stop();
    }
    cv_.notify_all();
  }
  workers_.clear();
}

void DownloadManager::retry_failed(std::size_t job_id) {
  std::lock_guard lock(mutex_);
  if (job_id >= jobs_.size() || hidden_jobs_[job_id] ||
      jobs_[job_id].status != DownloadStatus::Failed) {
    return;
  }
  jobs_[job_id].status = DownloadStatus::Queued;
  jobs_[job_id].error_message.clear();
  pending_.push_back(job_id);
  cv_.notify_one();
}

void DownloadManager::cancel(std::size_t job_id) {
  std::lock_guard lock(mutex_);
  if (job_id >= jobs_.size() || hidden_jobs_[job_id] ||
      is_terminal_status(jobs_[job_id].status)) {
    return;
  }
  jobs_[job_id].status = DownloadStatus::Cancelled;
}

std::size_t DownloadManager::clear_failed_and_skipped() {
  std::lock_guard lock(mutex_);
  std::size_t cleared = 0;
  for (std::size_t i = 0; i < jobs_.size(); ++i) {
    if (hidden_jobs_[i]) {
      continue;
    }
    if (jobs_[i].status == DownloadStatus::Failed ||
        jobs_[i].status == DownloadStatus::Skipped) {
      hidden_jobs_[i] = true;
      ++cleared;
    }
  }
  return cleared;
}

void DownloadManager::set_content_rules(ContentRules rules) {
  std::lock_guard lock(mutex_);
  rules_ = std::move(rules);
}

std::vector<DownloadJob> DownloadManager::jobs() const {
  std::lock_guard lock(mutex_);
  std::vector<DownloadJob> visible_jobs;
  visible_jobs.reserve(jobs_.size());
  for (std::size_t i = 0; i < jobs_.size(); ++i) {
    if (!hidden_jobs_[i]) {
      visible_jobs.push_back(jobs_[i]);
    }
  }
  return visible_jobs;
}

void DownloadManager::worker_loop(std::stop_token stop_token) {
  while (!stop_token.stop_requested()) {
    std::size_t job_id = 0;
    {
      std::unique_lock lock(mutex_);
      cv_.wait(lock, stop_token,
               [&] { return !pending_.empty() || stop_token.stop_requested(); });
      if (stop_token.stop_requested()) {
        return;
      }
      job_id = pending_.front();
      pending_.pop_front();
      if (job_id >= jobs_.size() || jobs_[job_id].status == DownloadStatus::Cancelled) {
        continue;
      }
    }
    process_job(job_id);
  }
}

void DownloadManager::process_job(std::size_t job_id) {
  std::string reason;
  {
    std::lock_guard lock(mutex_);
    if (job_id >= jobs_.size()) {
      return;
    }
    if (!is_post_allowed(jobs_[job_id].post, rules_, &reason)) {
      jobs_[job_id].status = DownloadStatus::Skipped;
      jobs_[job_id].error_message = reason;
      return;
    }
    if (index_.contains_duplicate(jobs_[job_id].post.hash, jobs_[job_id].post.provider,
                                  jobs_[job_id].post.id,
                                  jobs_[job_id].post.file_url)) {
      jobs_[job_id].status = DownloadStatus::Skipped;
      jobs_[job_id].error_message = "duplicate archive item";
      return;
    }
    jobs_[job_id].status = DownloadStatus::Downloading;
  }

  for (int attempt = 0; attempt <= options_.max_retries; ++attempt) {
    try {
      DownloadJob snapshot;
      {
        std::lock_guard lock(mutex_);
        snapshot = jobs_.at(job_id);
      }
      const auto url = selected_url(snapshot.post);
      if (url.empty()) {
        throw std::runtime_error("post has no downloadable URL for selected quality");
      }

      http_.download_to_file(
          url, snapshot.temp_path, true,
          [this, job_id](const HttpDownloadProgress& progress) {
            update_job(job_id, [&](DownloadJob& job) {
              job.bytes_downloaded = progress.downloaded;
              job.total_bytes = progress.total;
              job.speed_bytes_per_second = progress.speed_bytes_per_second;
            });
          });

      ensure_directory(snapshot.destination_path.parent_path());
      std::filesystem::rename(snapshot.temp_path, snapshot.destination_path);
      snapshot.destination_path = std::filesystem::absolute(snapshot.destination_path);
      snapshot.status = DownloadStatus::Complete;
      snapshot.error_message.clear();

      LocalArchiveItem item = archive_item_from_job(snapshot);
      index_.upsert(item);

      update_job(job_id, [&](DownloadJob& job) {
        job.status = DownloadStatus::Complete;
        job.error_message.clear();
        if (std::filesystem::exists(job.destination_path)) {
          job.bytes_downloaded =
              static_cast<std::int64_t>(std::filesystem::file_size(job.destination_path));
          job.total_bytes = job.bytes_downloaded;
        }
      });
      return;
    } catch (const std::exception& error) {
      update_job(job_id, [&](DownloadJob& job) {
        job.retry_count = attempt;
        job.error_message = error.what();
      });
      if (attempt == options_.max_retries) {
        update_job(job_id,
                   [](DownloadJob& job) { job.status = DownloadStatus::Failed; });
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(250 * (1 << attempt)));
    }
  }
}

std::string DownloadManager::selected_url(const Post& post) const {
  switch (options_.quality) {
    case DownloadQuality::Original:
      return post.file_url.empty() ? post.sample_url : post.file_url;
    case DownloadQuality::Sample:
      return post.sample_url.empty() ? post.file_url : post.sample_url;
    case DownloadQuality::Preview:
      if (!post.preview_url.empty()) {
        return post.preview_url;
      }
      if (!post.thumbnail_url.empty()) {
        return post.thumbnail_url;
      }
      return post.sample_url.empty() ? post.file_url : post.sample_url;
  }
  return post.file_url;
}

LocalArchiveItem DownloadManager::archive_item_from_job(
    const DownloadJob& job) const {
  LocalArchiveItem item;
  item.local_file_path = job.destination_path;
  item.provider = job.post.provider;
  item.post_id = job.post.id;
  item.post_url = job.post.post_url;
  item.file_url = job.post.file_url;
  item.hash = job.post.hash;
  item.tags = job.post.tags;
  item.artists = job.post.artist_tags;
  item.rating = job.post.rating;
  item.width = job.post.width;
  item.height = job.post.height;
  item.file_ext = job.post.file_ext;
  item.file_size =
      std::filesystem::exists(job.destination_path)
          ? static_cast<std::int64_t>(std::filesystem::file_size(job.destination_path))
          : job.post.file_size;
  item.downloaded_at = now_iso8601();
  return item;
}

void DownloadManager::update_job(
    std::size_t job_id, const std::function<void(DownloadJob&)>& fn) {
  std::lock_guard lock(mutex_);
  if (job_id < jobs_.size()) {
    fn(jobs_[job_id]);
  }
}

}  // namespace boorubox
