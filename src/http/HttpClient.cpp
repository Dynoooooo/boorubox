#include "http/HttpClient.hpp"

#include <curl/curl.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string_view>

#include "http/CurlHandle.hpp"
#include "util/PathUtil.hpp"
#include "util/StringUtil.hpp"

namespace boorubox {

namespace {

std::size_t write_string_callback(char* ptr, std::size_t size,
                                  std::size_t nmemb, void* userdata) {
  auto* body = static_cast<std::string*>(userdata);
  const auto bytes = size * nmemb;
  body->append(ptr, bytes);
  return bytes;
}

struct DownloadContext {
  std::FILE* file = nullptr;
  bool resume_requested = false;
  long status_code = 0;
  bool status_known = false;
  bool file_reset_done = false;
  bool write_error = false;
};

std::size_t header_callback(char* ptr, std::size_t size, std::size_t nmemb,
                            void* userdata) {
  auto* context = static_cast<DownloadContext*>(userdata);
  const auto bytes = size * nmemb;
  if (context == nullptr) {
    return bytes;
  }
  const std::string_view line(ptr, bytes);
  // Status lines look like "HTTP/1.1 206 Partial Content\r\n". When the server
  // follows a redirect, curl will deliver more than one status line; we keep
  // the last one, matching CURLINFO_RESPONSE_CODE.
  if (line.size() >= 5 &&
      (line.starts_with("HTTP/") || line.starts_with("http/"))) {
    const auto space = line.find(' ');
    if (space != std::string_view::npos && space + 1 < line.size()) {
      context->status_code =
          std::strtol(line.data() + space + 1, nullptr, 10);
      context->status_known = true;
      // A follow-up status may flip us back: reset the "already truncated"
      // flag so a 200 on the final hop is still handled correctly.
      context->file_reset_done = false;
    }
  }
  return bytes;
}

std::size_t write_file_callback(char* ptr, std::size_t size, std::size_t nmemb,
                                void* userdata) {
  auto* context = static_cast<DownloadContext*>(userdata);
  const auto bytes = size * nmemb;
  if (context == nullptr || context->file == nullptr) {
    return 0;
  }

  // First chunk after a status line: if the server ignored our Range request
  // and returned a full body (200 OK), truncate any existing bytes so we do
  // not concatenate stale partial data with the fresh full download.
  if (context->resume_requested && context->status_known &&
      !context->file_reset_done) {
    if (context->status_code != 0 && context->status_code != 206) {
      if (std::fflush(context->file) != 0) {
        context->write_error = true;
        return 0;
      }
      if (std::fseek(context->file, 0, SEEK_SET) != 0) {
        context->write_error = true;
        return 0;
      }
      if (::ftruncate(::fileno(context->file), 0) != 0) {
        context->write_error = true;
        return 0;
      }
    }
    context->file_reset_done = true;
  }

  const auto written = std::fwrite(ptr, 1, bytes, context->file);
  if (written != bytes) {
    context->write_error = true;
  }
  return written;
}

struct ProgressContext {
  const std::function<void(const HttpDownloadProgress&)>* callback = nullptr;
  const std::function<bool()>* should_cancel = nullptr;
  CURL* handle = nullptr;
};

int xferinfo_callback(void* userdata, curl_off_t dltotal, curl_off_t dlnow,
                      curl_off_t, curl_off_t) {
  auto* context = static_cast<ProgressContext*>(userdata);
  if (context == nullptr) {
    return 0;
  }
  if (context->should_cancel != nullptr && *context->should_cancel &&
      (*context->should_cancel)()) {
    return 1;
  }
  if (context->callback == nullptr || !*context->callback) {
    return 0;
  }

  double speed = 0.0;
  if (context->handle != nullptr) {
    curl_off_t speed_off = 0;
    curl_easy_getinfo(context->handle, CURLINFO_SPEED_DOWNLOAD_T, &speed_off);
    speed = static_cast<double>(speed_off);
  }
  try {
    (*context->callback)(HttpDownloadProgress{
        .downloaded = static_cast<std::int64_t>(dlnow),
        .total = static_cast<std::int64_t>(dltotal),
        .speed_bytes_per_second = speed,
    });
  } catch (const std::exception&) {
    return 1;
  }
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
}

}  // namespace

HttpClient::HttpClient(std::string user_agent)
    : user_agent_(std::move(user_agent)) {}

HttpRequestCancelled::HttpRequestCancelled()
    : std::runtime_error("download cancelled") {}

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
  // Small API responses should never take longer than ~2 minutes; kill them
  // hard if they do. Large file downloads use a low-speed watchdog instead.
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L),
                      "set timeout");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                       write_string_callback),
                      "set write callback");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body),
                      "set write data");

  throw_on_curl_error(curl_easy_perform(curl),
                      "GET " + redact_url_secrets(url));

  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  char* effective_url = nullptr;
  curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
  if (status >= 400) {
    throw std::runtime_error("HTTP " + std::to_string(status) + " for " +
                             redact_url_secrets(url));
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
    const std::function<void(const HttpDownloadProgress&)>& progress,
    const std::function<bool()>& should_cancel) const {
  if (should_cancel && should_cancel()) {
    throw HttpRequestCancelled();
  }
  ensure_directory(output_path.parent_path());

  curl_off_t resume_from = 0;
  if (resume && std::filesystem::exists(output_path)) {
    resume_from = static_cast<curl_off_t>(std::filesystem::file_size(output_path));
  }
  const bool resume_requested = resume_from > 0;

  // "r+b" lets us seek to the end and (if the server ignores Range) rewind to
  // the start and truncate without closing/reopening. When there is no resume
  // we create a fresh file with "wb".
  std::FILE* file = std::fopen(output_path.string().c_str(),
                               resume_requested ? "r+b" : "wb");
  if (file == nullptr && resume_requested) {
    // The file disappeared between the existence check and fopen; fall back
    // to a fresh download.
    resume_from = 0;
    file = std::fopen(output_path.string().c_str(), "wb");
  }
  if (file == nullptr) {
    throw std::runtime_error("failed to open " + output_path.string());
  }
  if (resume_requested) {
    if (std::fseek(file, 0, SEEK_END) != 0) {
      std::fclose(file);
      throw std::runtime_error("failed to seek " + output_path.string());
    }
  }

  DownloadContext download_context{
      .file = file,
      .resume_requested = resume_requested,
  };

  CurlEasy easy;
  auto* curl = easy.get();
  configure_common(curl, user_agent_, url);
  // Large file downloads should not be killed by a hard total-transfer
  // timeout. Instead, abort if the connection goes idle for a sustained
  // period.
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L),
                      "set low-speed limit");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L),
                      "set low-speed time");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                       write_file_callback),
                      "set write callback");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_WRITEDATA,
                                       &download_context),
                      "set write data");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
                                       header_callback),
                      "set header callback");
  throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_HEADERDATA,
                                       &download_context),
                      "set header data");
  if (resume_requested) {
    throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE,
                                         resume_from),
                        "set resume offset");
  }

  ProgressContext progress_context{
      .callback = &progress,
      .should_cancel = &should_cancel,
      .handle = curl,
  };
  if (progress || should_cancel) {
    throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L),
                        "enable progress");
    throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
                                         xferinfo_callback),
                        "set progress callback");
    throw_on_curl_error(curl_easy_setopt(curl, CURLOPT_XFERINFODATA,
                                         &progress_context),
                        "set progress data");
  }

  const auto code = curl_easy_perform(curl);
  std::fflush(file);
  std::fclose(file);

  if (code == CURLE_ABORTED_BY_CALLBACK && should_cancel && should_cancel()) {
    throw HttpRequestCancelled();
  }
  if (download_context.write_error) {
    throw std::runtime_error("failed to write " + output_path.string());
  }
  throw_on_curl_error(code, "download " + redact_url_secrets(url));
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  if (status >= 400) {
    throw std::runtime_error("HTTP " + std::to_string(status) + " for " +
                             redact_url_secrets(url));
  }
}

const std::string& HttpClient::user_agent() const {
  return user_agent_;
}

}  // namespace boorubox
