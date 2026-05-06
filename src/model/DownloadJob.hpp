#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include "model/Post.hpp"

namespace boorubox {

enum class DownloadStatus {
  Queued,
  Downloading,
  Paused,
  Complete,
  Skipped,
  Failed,
  Cancelled,
};

enum class DownloadQuality {
  Original,
  Sample,
  Preview,
};

struct DownloadJob {
  std::size_t id = 0;
  Post post;
  std::filesystem::path destination_path;
  std::filesystem::path temp_path;
  DownloadStatus status = DownloadStatus::Queued;
  std::int64_t bytes_downloaded = 0;
  std::int64_t total_bytes = 0;
  double speed_bytes_per_second = 0.0;
  int retry_count = 0;
  std::string error_message;
  std::chrono::system_clock::time_point queued_at =
      std::chrono::system_clock::now();
};

std::string to_string(DownloadStatus status);

}  // namespace boorubox
