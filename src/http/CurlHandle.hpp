#pragma once

#include <curl/curl.h>

#include <stdexcept>
#include <string_view>

namespace boorubox {

class CurlGlobal {
 public:
  static void ensure_initialized();

 private:
  CurlGlobal();
  ~CurlGlobal();
};

class CurlEasy {
 public:
  CurlEasy();
  ~CurlEasy();

  CurlEasy(const CurlEasy&) = delete;
  CurlEasy& operator=(const CurlEasy&) = delete;
  CurlEasy(CurlEasy&&) = delete;
  CurlEasy& operator=(CurlEasy&&) = delete;

  CURL* get() const;

 private:
  CURL* handle_ = nullptr;
};

void throw_on_curl_error(CURLcode code, std::string_view context);

}  // namespace boorubox
