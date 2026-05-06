#include "app/Config.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>

#include "util/PathUtil.hpp"
#include "util/StringUtil.hpp"

namespace boorubox {

namespace {

std::string strip_comment(std::string_view line) {
  bool in_quotes = false;
  std::string out;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char ch = line[i];
    if (ch == '"' && (i == 0 || line[i - 1] != '\\')) {
      in_quotes = !in_quotes;
    }
    if (ch == '#' && !in_quotes) {
      break;
    }
    out.push_back(ch);
  }
  return trim(out);
}

std::string unquote(std::string value) {
  value = trim(value);
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    value = value.substr(1, value.size() - 2);
  }
  return replace_all(value, "\\\"", "\"");
}

bool parse_bool(std::string_view value) {
  const auto token = lower(trim(value));
  if (token == "true" || token == "yes" || token == "1" || token == "on") {
    return true;
  }
  if (token == "false" || token == "no" || token == "0" || token == "off") {
    return false;
  }
  throw std::runtime_error("invalid bool value: " + std::string(value));
}

int parse_positive_int(std::string_view value, std::string_view key) {
  const auto token = trim(value);
  std::size_t consumed = 0;
  int parsed = 0;
  try {
    parsed = std::stoi(token, &consumed);
  } catch (const std::exception&) {
    throw std::runtime_error("invalid integer for " + std::string(key) +
                             ": " + std::string(value));
  }
  if (consumed != token.size() || parsed <= 0) {
    throw std::runtime_error("invalid positive integer for " +
                             std::string(key) + ": " + std::string(value));
  }
  return parsed;
}

Rating parse_rating_config(std::string value) {
  value = unquote(value);
  const auto rating = rating_from_string(value);
  if (rating == Rating::Unknown && lower(trim(value)) != "unknown") {
    throw std::runtime_error("invalid rating value: " + value);
  }
  return rating;
}

std::string parse_download_quality(std::string value) {
  value = lower(unquote(value));
  if (value != "original" && value != "sample" && value != "preview") {
    throw std::runtime_error("invalid preferred_download_quality: " + value);
  }
  return value;
}

std::string quote(std::string_view value) {
  return "\"" + replace_all(std::string(value), "\"", "\\\"") + "\"";
}

std::string config_path_string(const std::filesystem::path& path) {
  return path.string();
}

void write_string_array(std::ostream& out,
                        const std::vector<std::string>& values) {
  out << "[\n";
  for (const auto& value : values) {
    out << "  " << quote(value) << ",\n";
  }
  out << "]";
}

std::vector<std::string> parse_string_array(std::string value) {
  value = trim(value);
  if (!value.empty() && value.front() == '[') {
    value.erase(value.begin());
  }
  if (!value.empty() && value.back() == ']') {
    value.pop_back();
  }

  std::vector<std::string> out;
  std::string current;
  bool in_quotes = false;
  for (std::size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if (ch == '"' && (i == 0 || value[i - 1] != '\\')) {
      in_quotes = !in_quotes;
      current.push_back(ch);
      continue;
    }
    if (ch == ',' && !in_quotes) {
      auto item = trim(current);
      if (!item.empty()) {
        out.push_back(unquote(item));
      }
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  auto item = trim(current);
  if (!item.empty()) {
    out.push_back(unquote(item));
  }
  return out;
}

void set_provider_value(ProviderConfig& provider, std::string_view key,
                        const std::string& value) {
  if (key == "enabled") {
    provider.enabled = parse_bool(value);
  } else if (key == "base_url") {
    provider.base_url = unquote(value);
  } else if (key == "nsfw_provider") {
    provider.nsfw_provider = parse_bool(value);
  } else if (key == "login") {
    provider.login = unquote(value);
  } else if (key == "api_key") {
    provider.api_key = unquote(value);
  }
}

void set_value(AppConfig& config, const std::string& section,
               const std::string& key, const std::string& value) {
  if (section == "app") {
    if (key == "download_dir") {
      config.download_dir = expand_user_path(unquote(value));
    } else if (key == "cache_dir") {
      config.cache_dir = expand_user_path(unquote(value));
    } else if (key == "index_path") {
      config.index_path = expand_user_path(unquote(value));
    } else if (key == "user_agent") {
      config.user_agent = unquote(value);
    } else if (key == "max_concurrent_downloads") {
      config.max_concurrent_downloads =
          parse_positive_int(value, "max_concurrent_downloads");
    } else if (key == "per_site_delay_ms") {
      config.per_site_delay_ms = parse_positive_int(value, "per_site_delay_ms");
    } else if (key == "preferred_preview_backend") {
      config.preferred_preview_backend = unquote(value);
    } else if (key == "preferred_download_quality") {
      config.preferred_download_quality = parse_download_quality(value);
    } else if (key == "enable_nsfw") {
      config.enable_nsfw = parse_bool(value);
    }
    return;
  }

  if (section == "filters") {
    if (key == "default_rating") {
      config.default_rating = parse_rating_config(value);
    } else if (key == "blacklisted_tags") {
      config.blacklisted_tags = parse_string_array(value);
    }
    return;
  }

  if (section == "filenames" && key == "template") {
    config.filename_template = unquote(value);
    return;
  }

  constexpr std::string_view providers_prefix = "providers.";
  if (starts_with(section, providers_prefix)) {
    const auto provider_name = section.substr(providers_prefix.size());
    set_provider_value(config.providers[provider_name], key, value);
  }
}

}  // namespace

AppConfig Config::defaults() {
  AppConfig config;
  config.download_dir = expand_user_path("~/Pictures/BooruBox");
  config.cache_dir = cache_home() / "boorubox";
  config.index_path = data_home() / "boorubox/index.sqlite";
  config.user_agent =
      "BooruBox/0.1 (+https://example.invalid/boorubox; contact@example.invalid)";
  config.max_concurrent_downloads = 3;
  config.per_site_delay_ms = 1000;
  config.preferred_preview_backend = "kitty";
  config.preferred_download_quality = "original";
  config.enable_nsfw = false;
  config.default_rating = Rating::Safe;
  config.blacklisted_tags = {
      "gore",
      "loli",
      "shota",
      "young-looking",
      "minors",
  };
  config.filename_template = "{provider}/{artist}/{id}_{md5}.{ext}";

  config.providers["danbooru"] = ProviderConfig{
      .enabled = true,
      .base_url = "https://danbooru.donmai.us",
      .nsfw_provider = false,
      .login = "",
      .api_key = "",
  };
  config.providers["safebooru"] = ProviderConfig{
      .enabled = true,
      .base_url = "https://safebooru.org",
      .nsfw_provider = false,
      .login = "",
      .api_key = "",
  };
  config.providers["gelbooru"] = ProviderConfig{
      .enabled = true,
      .base_url = "https://gelbooru.com",
      .nsfw_provider = false,
      .login = "",
      .api_key = "",
  };
  config.providers["e926"] = ProviderConfig{
      .enabled = true,
      .base_url = "https://e926.net",
      .nsfw_provider = false,
      .login = "",
      .api_key = "",
  };
  config.providers["e621"] = ProviderConfig{
      .enabled = false,
      .base_url = "https://e621.net",
      .nsfw_provider = true,
      .login = "",
      .api_key = "",
  };
  config.providers["rule34"] = ProviderConfig{
      .enabled = false,
      .base_url = "https://rule34.xxx",
      .nsfw_provider = true,
      .login = "",
      .api_key = "",
  };
  return config;
}

AppConfig Config::load(const std::filesystem::path& path) {
  AppConfig config = defaults();
  std::ifstream file(path);
  if (!file) {
    return config;
  }

  std::string section = "app";
  std::string line;
  int line_number = 0;
  while (std::getline(file, line)) {
    ++line_number;
    line = strip_comment(line);
    if (line.empty()) {
      continue;
    }

    if (line.front() == '[' && line.back() == ']') {
      section = trim(std::string_view(line).substr(1, line.size() - 2));
      continue;
    }

    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const auto key = trim(std::string_view(line).substr(0, equals));
    auto value = trim(std::string_view(line).substr(equals + 1));

    if (value == "[") {
      std::string array_body = "[";
      while (std::getline(file, line)) {
        ++line_number;
        auto body_line = strip_comment(line);
        array_body += body_line;
        if (body_line.find(']') != std::string::npos) {
          break;
        }
      }
      value = array_body;
    }

    try {
      set_value(config, section, key, value);
    } catch (const std::exception& error) {
      throw std::runtime_error(path.string() + ":" +
                               std::to_string(line_number) + ": " +
                               error.what());
    }
  }
  return config;
}

void Config::save(const AppConfig& config, const std::filesystem::path& path) {
  ensure_directory(path.parent_path());
  const auto temp_path = path.string() + ".tmp";
  std::ofstream out(temp_path, std::ios::trunc);
  if (!out) {
    throw std::runtime_error("failed to write " + temp_path);
  }

  out << "[app]\n";
  out << "download_dir = " << quote(config_path_string(config.download_dir)) << "\n";
  out << "cache_dir = " << quote(config_path_string(config.cache_dir)) << "\n";
  out << "index_path = " << quote(config_path_string(config.index_path)) << "\n";
  out << "user_agent = " << quote(config.user_agent) << "\n";
  out << "max_concurrent_downloads = " << config.max_concurrent_downloads << "\n";
  out << "per_site_delay_ms = " << config.per_site_delay_ms << "\n";
  out << "preferred_preview_backend = "
      << quote(config.preferred_preview_backend) << "\n";
  out << "preferred_download_quality = "
      << quote(config.preferred_download_quality) << "\n";
  out << "enable_nsfw = " << (config.enable_nsfw ? "true" : "false") << "\n\n";

  out << "[filters]\n";
  out << "default_rating = " << quote(to_string(config.default_rating)) << "\n";
  out << "blacklisted_tags = ";
  write_string_array(out, config.blacklisted_tags);
  out << "\n\n";

  out << "[filenames]\n";
  out << "template = " << quote(config.filename_template) << "\n\n";

  for (const auto& [name, provider] : config.providers) {
    out << "[providers." << name << "]\n";
    out << "enabled = " << (provider.enabled ? "true" : "false") << "\n";
    out << "base_url = " << quote(provider.base_url) << "\n";
    out << "nsfw_provider = " << (provider.nsfw_provider ? "true" : "false")
        << "\n";
    if (!provider.login.empty()) {
      out << "login = " << quote(provider.login) << "\n";
    }
    if (!provider.api_key.empty()) {
      out << "api_key = " << quote(provider.api_key) << "\n";
    }
    out << "\n";
  }

  out.close();
  std::filesystem::rename(temp_path, path);
}

std::filesystem::path default_config_path() {
  return config_home() / "boorubox/config.toml";
}

std::filesystem::path nsfw_warning_ack_path() {
  return state_home() / "boorubox/nsfw-warning-accepted";
}

bool acknowledge_nsfw_warning_once(const AppConfig& config) {
  if (!config.enable_nsfw) {
    return true;
  }
  const auto ack_path = nsfw_warning_ack_path();
  if (std::filesystem::exists(ack_path)) {
    return true;
  }

  std::cerr
      << "BooruBox NSFW mode is enabled.\n"
      << "Only use providers and searches that are legal for you to access, "
         "respect each site's rules, and keep blacklists/rating filters active. "
         "BooruBox will not assist with illegal sexual content, sexualized "
         "minors, non-consensual sexual content, or bypassing site controls.\n"
      << "Type I AGREE to continue: ";

  std::string answer;
  std::getline(std::cin, answer);
  if (answer != "I AGREE") {
    return false;
  }

  ensure_directory(ack_path.parent_path());
  std::ofstream ack(ack_path);
  ack << "accepted\n";
  return true;
}

}  // namespace boorubox
