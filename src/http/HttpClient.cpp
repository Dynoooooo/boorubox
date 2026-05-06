#include "http/HttpClient.hpp"

#include <curl/curl.h>

#include <fstream>
#include <stdexcept>

#include "http/CurlHandle.hpp"
#include "util/PathUtil.hpp"

namespace boorubox {

namespace {

std::size_t write_string_callback(char* ptr, std::size_t size,
                                  std::size_t nmemb, void* userdata) {
  auto* body = static_cast<std::string*>(userdata);
  const auto bytes = size * nmemb;
  body->append(ptr, bytes);
  return bytes;
}

std::size_t write_file_callback(char* ptr, std::size_t size, std::size_t nmemb,
                                void* userdata) {
  auto* output = static_cast<std::ofstream*>(userdata);
  const auto bytes = static_cast<std::streamsize>(size * nmemb);
  output->write(ptr, bytes);
  return output->good() ? static_cast<std::size_t>(bytes) : 0;
}

struct ProgressContext {
  const std::function<void(const HttpDownloadProgress&)>* callback = nullptr;
  CURL* handle = nullptr;
};

int xferinfo_callback(void* userdata, curl_off_t dltotal, curl_off_t dlnow,
                      curl_off_t, curl_off_t) {
  auto* context = static_cast<ProgressContext*>(userdata);
  if (context == nullptr || context->callback == nullptr ||
      !*context->callback) {
    return 0;
  }

  double speed = 0.0;
  if (context->handle != nullptr) {
    curl_off_t speed_off = 0;
    curl_easy_getinfo(context->handle, CURLINFO_SPEED_DOWNLOAD_T, &speed_off);
    speed = static_cast<double>(speed_off);
  }
  (*context->callback)(HttpDownloadProgress{
      .downloaded = static_cast<std::int64_t>(dlnow),
      .total = static_cast<std::int64_t>(dltotal),
      .speed_bytes_per_second = speed,
  });
  return 0;
}

struct CurlHeaderList {
  curl_slist* list = nullptr;

  ~CurlHeaderList() {
    if (list != nullptr) {
      curl_slist_free_all(list);
    }
  }

  void append(const std::string& header) {
    list = curl_slist_append(list, header.c_str());
  }
};

void configure_common(CURL* curl, const std::string& user_agent,
                      const std::string& url) {
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_URL, url.c_str()),
                      "set URL");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_USERAGENT,
                                       user_agent.c_str()),
                      "set User-Agent");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L),
                      "set follow redirects");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L),
                      "set max redirects");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L),
                      "set fail-on-error");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L),
                      "set connect timeout");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L),
                      "set timeout");
}

}  // namespace

HttpClient::HttpClient(std::string user_agent)
    : user_agent_(std::move(user_agent)) {}

HttpResponse HttpClient::get(const std::string& url,
                             const std::vector<std::string>& headers) const {
  CurlEasy easy;
  auto* curl = easy.get();
  std::string body;
  CurlHeaderList header_list;
  for (const auto& header : headers) {
    header_list.append(header);
  }
  if (header_list.list != nullptr) {
    throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_HTTPHEADER,
                                         header_list.list),
                        "set headers");
  }
  configure_common(curl, user_agent_, url);
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                       write_string_callback),
                      "set write callback");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body),
                      "set write data");

  throw_on_curl_error(curl_easy_perform(curl), "GET " + url);

  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  char* effective_url = nullptr;
  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
  if (status >= 400) {
    throw std::runtime_error("HTTP " + std::to_string(status) + " for " + url);
  }
  return HttpResponse{
      .status_code = status,
      .body = std::move(body),
      .effective_url = effective_url == nullptr ? url : effective_url,
  };
}

void HttpClient::download_to_file(
    const std::string& url, const std::filesystem::path& output_path,
    bool resume,
    const std::function<void(const HttpDownloadProgress&)>& progress) const {
  ensure_directory(output_path.parent_path());
  std::ios::openmode mode = std::ios::binary;
  curl_off_t resume_from = 0;
  if (resume && std::filesystem::exists(output_path)) {
    resume_from = static_cast<curl_off_t>(std::filesystem::file_size(output_path));
    if (resume_from > 0) {
      mode |= std::ios::app;
    } else {
      mode |= std::ios::trunc;
    }
  } else {
    mode |= std::ios::trunc;
  }

  std::ofstream output(output_path, mode);
  if (!output) {
    throw std::runtime_error("failed to open " + output_path.string());
  }

  CurlEasy easy;
  auto* curl = easy.get();
  configure_common(curl, user_agent_, url);
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                       write_file_callback),
                      "set write callback");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output),
                      "set write data");
  if (resume_from > 0) {
    throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE,
                                         resume_from),
                        "set resume offset");
  }

  ProgressContext progress_context{
      .callback = &progress,
      .handle = curl,
  };
  if (progress) {
    throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L),
                        "enable progress");
    throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
                                         xferinfo_callback),
                        "set progress callback");
    throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_XFERINFODATA,
                                         &progress_context),
                        "set progress data");
  }

  throw_on_curl_error(curl_easy_perform(curl), "download " + url);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  if (status >= 400) {
    throw std::runtime_error("HTTP " + std::to_string(status) + " for " + url);
  }
}

const std::string& HttpClient::user_agent() const {
  return user_agent_;
}

}  // namespace boorubox
