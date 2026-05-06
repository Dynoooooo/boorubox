#include "util/PathUtil.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>

#include "util/StringUtil.hpp"

namespace boorubox {

namespace {

std::filesystem::path env_path(const char* name,
                               const std::filesystem::path& fallback) {
  if (const char* value = std::getenv(name); value != nullptr && *value != 0) {
    return std::filesystem::path(value);
  }
  return fallback;
}

std::filesystem::path home_path() {
  if (const char* value = std::getenv("HOME"); value != nullptr && *value != 0) {
    return std::filesystem::path(value);
  }
  return std::filesystem::current_path();
}

}  // namespace

std::filesystem::path expand_user_path(std::string_view value) {
  const std::string text(value);
  if (text == "~") {
    return home_path();
  }
  if (starts_with(text, "~/")) {
    return home_path() / text.substr(2);
  }
  return std::filesystem::path(text);
}

void ensure_directory(const std::filesystem::path& path) {
  if (path.empty()) {
    return;
  }
  std::error_code error;
  std::filesystem::create_directories(path, error);
  if (error) {
    throw std::runtime_error("failed to create directory " + path.string() +
                             ": " + error.message());
  }
}

std::filesystem::path cache_home() {
  return env_path("XDG_CACHE_HOME", home_path() / ".cache");
}

std::filesystem::path config_home() {
  return env_path("XDG_CONFIG_HOME", home_path() / ".config");
}

std::filesystem::path data_home() {
  return env_path("XDG_DATA_HOME", home_path() / ".local/share");
}

std::filesystem::path state_home() {
  return env_path("XDG_STATE_HOME", home_path() / ".local/state");
}

std::string extension_from_url(std::string_view url) {
  std::string text(url);
  if (const auto query = text.find('?'); query != std::string::npos) {
    text.resize(query);
  }
  const auto slash = text.find_last_of('/');
  const auto dot = text.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
    return {};
  }
  return lower(text.substr(dot + 1));
}

}  // namespace boorubox
