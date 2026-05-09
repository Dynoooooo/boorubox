#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unistd.h>

#include "download/DownloadManager.hpp"
#include "http/HttpClient.hpp"
#include "index/JsonlArchiveIndex.hpp"
#include "model/Post.hpp"

using namespace boorubox;

namespace {

std::filesystem::path temp_download_root(std::string_view name) {
  const auto dir = std::filesystem::temp_directory_path() /
                   ("boorubox_dl_test_" + std::to_string(::getpid()) + "_" +
                    std::string(name));
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

Post make_post(std::string id, std::string hash,
               std::string file_url = "https://cdn.example/p.png") {
  Post post;
  post.provider = "danbooru";
  post.id = std::move(id);
  post.hash = std::move(hash);
  post.file_url = std::move(file_url);
  post.file_ext = "png";
  post.rating = Rating::Safe;
  return post;
}

struct Fixture {
  std::filesystem::path root;
  std::filesystem::path download_dir;
  std::filesystem::path index_path;
  HttpClient http;
  JsonlArchiveIndex index;
  std::shared_ptr<std::mutex> index_mutex;
  DownloadManager manager;

  explicit Fixture(std::string_view name)
      : root(temp_download_root(name)),
        download_dir(root / "downloads"),
        index_path(root / "index.jsonl"),
        http("BooruBoxTest/0.0"),
        index(index_path),
        index_mutex(std::make_shared<std::mutex>()),
        manager(http, index, ContentRules{}, index_mutex,
                DownloadOptions{
                    .download_dir = download_dir,
                    .filename_template = "{provider}/{id}.{ext}",
                    .max_concurrent_downloads = 1,
                }) {}

  ~Fixture() {
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
};

}  // namespace

TEST_CASE("DownloadManager enqueue assigns sequential ids and tracks jobs") {
  Fixture fx("enqueue_ids");

  const auto first = fx.manager.enqueue(make_post("1", "hash-a"));
  const auto second = fx.manager.enqueue(make_post("2", "hash-b"));

  REQUIRE(first == 0);
  REQUIRE(second == 1);

  const auto jobs = fx.manager.jobs();
  REQUIRE(jobs.size() == 2);
  REQUIRE(jobs[0].status == DownloadStatus::Queued);
  REQUIRE(jobs[1].status == DownloadStatus::Queued);
  REQUIRE(jobs[0].destination_path.filename() == "1.png");
  REQUIRE(jobs[1].destination_path.filename() == "2.png");
}

TEST_CASE("DownloadManager refuses duplicates already in the archive index") {
  Fixture fx("dup_archive");

  // Seed the index with a matching item. The manager should detect this when
  // processing the job and mark it skipped rather than spending a worker.
  LocalArchiveItem existing;
  existing.provider = "danbooru";
  existing.post_id = "42";
  existing.hash = "dup-hash";
  existing.local_file_path = fx.download_dir / "existing.png";
  fx.index.upsert(existing);

  const auto job_id = fx.manager.enqueue(make_post("42", "dup-hash"));
  fx.manager.start();
  // Give the worker thread a chance to observe and skip the duplicate.
  for (int i = 0; i < 100; ++i) {
    const auto jobs = fx.manager.jobs();
    if (!jobs.empty() && jobs[0].status == DownloadStatus::Skipped) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  fx.manager.stop();

  const auto jobs = fx.manager.jobs();
  REQUIRE(jobs.size() == 1);
  REQUIRE(jobs[0].id == job_id);
  REQUIRE(jobs[0].status == DownloadStatus::Skipped);
  REQUIRE(jobs[0].error_message.find("duplicate") != std::string::npos);
}

TEST_CASE("DownloadManager clear_failed_and_skipped hides terminal jobs") {
  Fixture fx("clear_failed");

  LocalArchiveItem existing;
  existing.provider = "danbooru";
  existing.post_id = "1";
  existing.hash = "same";
  existing.local_file_path = fx.download_dir / "existing.png";
  fx.index.upsert(existing);

  fx.manager.enqueue(make_post("1", "same"));
  fx.manager.start();
  for (int i = 0; i < 100; ++i) {
    const auto jobs = fx.manager.jobs();
    if (!jobs.empty() && jobs[0].status == DownloadStatus::Skipped) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  fx.manager.stop();

  REQUIRE(fx.manager.jobs().size() == 1);
  const auto cleared = fx.manager.clear_failed_and_skipped();
  REQUIRE(cleared == 1);
  REQUIRE(fx.manager.jobs().empty());
}

TEST_CASE("DownloadManager cancel marks queued jobs cancelled before running") {
  Fixture fx("cancel_queued");

  const auto job_id = fx.manager.enqueue(make_post("7", "hash-cancel"));
  // Cancel without starting workers so we exercise the pre-run branch.
  fx.manager.cancel(job_id);

  const auto jobs = fx.manager.jobs();
  REQUIRE(jobs.size() == 1);
  REQUIRE(jobs[0].status == DownloadStatus::Cancelled);
}

TEST_CASE("DownloadManager rejects posts blocked by content rules") {
  const auto root = temp_download_root("content_rules");
  const auto download_dir = root / "downloads";
  HttpClient http("BooruBoxTest/0.0");
  JsonlArchiveIndex index(root / "index.jsonl");
  auto index_mutex = std::make_shared<std::mutex>();

  ContentRules rules;
  rules.enable_nsfw = false;
  rules.blacklisted_tags = {"gore"};
  DownloadManager manager(http, index, rules, index_mutex,
                          DownloadOptions{
                              .download_dir = download_dir,
                              .filename_template = "{provider}/{id}.{ext}",
                              .max_concurrent_downloads = 1,
                          });

  auto post = make_post("13", "hash-blocked");
  post.tags = {"gore", "landscape"};
  const auto job_id = manager.enqueue(post);

  manager.start();
  for (int i = 0; i < 100; ++i) {
    const auto jobs = manager.jobs();
    if (!jobs.empty() && jobs[0].status == DownloadStatus::Skipped) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  manager.stop();

  const auto jobs = manager.jobs();
  REQUIRE(jobs.size() == 1);
  REQUIRE(jobs[0].id == job_id);
  REQUIRE(jobs[0].status == DownloadStatus::Skipped);
  REQUIRE(jobs[0].error_message.find("blacklist") != std::string::npos);

  std::error_code ignored;
  std::filesystem::remove_all(root, ignored);
}
