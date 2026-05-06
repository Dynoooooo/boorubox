#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace boorubox {

struct HttpResponse {
  long status_code = 0;
  std::string body;
  std::string effective_url;
};

struct HttpDownloadProgress {
  std::int64_t downloaded = 0;
  std::int64_t total = 0;
  double speed_bytes_per_second = 0.0;
};

class HttpClient {
 public:
  explicit HttpClient(std::string user_agent);

  HttpResponse get(const std::string& url,
                   const std::vector<std::string>& headers = {}) const;

  void download_to_file(
      const std::string& url, const std::filesystem::path& output_path,
      bool resume,
      const std::function<void(const HttpDownloadProgress&)>& progress = {})
      const;

  const std::string& user_agent() const;

 private:
  std::string user_agent_;
};

}  // namespace boorubox
