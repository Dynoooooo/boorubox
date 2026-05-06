#include "model/DownloadJob.hpp"

namespace boorubox {

std::string to_string(DownloadStatus status) {
  switch (status) {
    case DownloadStatus::Queued:
      return "queued";
    case DownloadStatus::Downloading:
      return "downloading";
    case DownloadStatus::Paused:
      return "paused";
    case DownloadStatus::Complete:
      return "complete";
    case DownloadStatus::Skipped:
      return "skipped";
    case DownloadStatus::Failed:
      return "failed";
    case DownloadStatus::Cancelled:
      return "cancelled";
  }
  return "unknown";
}

}  // namespace boorubox
