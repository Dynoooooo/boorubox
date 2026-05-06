#include "http/CurlHandle.hpp"

namespace boorubox {

void CurlGlobal::ensure_initialized() {
  static CurlGlobal global;
}

CurlGlobal::CurlGlobal() {
  const auto code = curl_global_init(CURL_GLOBAL_DEFAULT);
  if (code != CURLE_OK) {
    throw std::runtime_error("curl_global_init failed");
  }
}

CurlGlobal::~CurlGlobal() {
  curl_global_cleanup();
}

CurlEasy::CurlEasy() {
  CurlGlobal::ensure_initialized();
  handle_ = curl_easy_init();
  if (handle_ == nullptr) {
    throw std::runtime_error("curl_easy_init failed");
  }
}

CurlEasy::~CurlEasy() {
  if (handle_ != nullptr) {
    curl_easy_cleanup(handle_);
  }
}

CURL* CurlEasy::get() const {
  return handle_;
}

void throw_on_curl_error(CURLcode code, std::string_view context) {
  if (code != CURLE_OK) {
    throw std::runtime_error(std::string(context) + ": " +
                             curl_easy_strerror(code));
  }
}

}  // namespace boorubox
