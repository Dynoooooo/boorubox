#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "model/Rating.hpp"

namespace boorubox {

struct ProviderConfig {
  bool enabled = true;
  std::string base_url;
  bool nsfw_provider = false;
  std::string login;
  std::string api_key;
};

struct AppConfig {
  std::filesystem::path download_dir;
  std::filesystem::path cache_dir;
  std::filesystem::path index_path;
  std::string user_agent;
  int max_concurrent_downloads = 3;
  int per_site_delay_ms = 1000;
  std::string preferred_download_quality = "original";
  bool enable_nsfw = false;

  Rating default_rating = Rating::Safe;
  std::vector<std::string> blacklisted_tags;

  std::string filename_template = "{provider}/{artist}/{id}_{md5}.{ext}";
  std::map<std::string, ProviderConfig> providers;
};

class Config {
 public:
  static AppConfig defaults();
  static AppConfig load(const std::filesystem::path& path);
  static void save(const AppConfig& config, const std::filesystem::path& path);
};

std::filesystem::path default_config_path();
std::filesystem::path nsfw_warning_ack_path();
bool acknowledge_nsfw_warning_once(const AppConfig& config);

}  // namespace boorubox
