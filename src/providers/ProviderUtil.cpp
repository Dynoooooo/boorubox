#include "providers/ProviderUtil.hpp"

#include <algorithm>
#include <charconv>

#include "util/StringUtil.hpp"

namespace boorubox::providers {

std::string append_query_params(
    std::string base,
    const std::vector<std::pair<std::string, std::string>>& params) {
  base.push_back(base.find('?') == std::string::npos ? '?' : '&');
  for (std::size_t i = 0; i < params.size(); ++i) {
    if (i != 0) {
      base.push_back('&');
    }
    base += url_encode(params[i].first);
    base.push_back('=');
    base += url_encode(params[i].second);
  }
  return base;
}

std::string without_trailing_slash(std::string value) {
  while (!value.empty() && value.back() == '/') {
    value.pop_back();
  }
  return value;
}

std::string absolutize_url(std::string_view base_url, std::string_view value) {
  if (value.empty()) {
    return {};
  }
  const std::string text(value);
  if (starts_with(text, "http://") || starts_with(text, "https://")) {
    return text;
  }
  if (starts_with(text, "//")) {
    return "https:" + text;
  }
  const auto base = without_trailing_slash(std::string(base_url));
  if (starts_with(text, "/")) {
    return base + text;
  }
  return base + "/" + text;
}

std::vector<std::string> tag_terms_with_exclusions(
    const SearchQuery& query, const SearchSafety& safety) {
  std::vector<std::string> terms = query.tags;
  for (const auto& tag : query.excluded_tags) {
    if (!tag.empty()) {
      terms.push_back("-" + tag);
    }
  }
  for (const auto& tag : safety.blacklisted_tags) {
    if (!tag.empty()) {
      terms.push_back("-" + tag);
    }
  }
  return terms;
}

std::string joined_tag_terms(const std::vector<std::string>& terms) {
  return join(terms, " ");
}

int clamp_limit(int value, int max_limit) {
  return std::clamp(value <= 0 ? 20 : value, 1, max_limit);
}

std::string json_string(const nlohmann::json& object, std::string_view key) {
  const auto it = object.find(std::string(key));
  if (it == object.end() || it->is_null()) {
    return {};
  }
  if (it->is_string()) {
    return it->get<std::string>();
  }
  if (it->is_number_integer() || it->is_number_unsigned()) {
    return std::to_string(it->get<std::int64_t>());
  }
  if (it->is_number_float()) {
    return std::to_string(it->get<double>());
  }
  return {};
}

std::int64_t json_int64(const nlohmann::json& object, std::string_view key) {
  const auto it = object.find(std::string(key));
  if (it == object.end() || it->is_null()) {
    return 0;
  }
  if (it->is_number_integer() || it->is_number_unsigned()) {
    return it->get<std::int64_t>();
  }
  if (it->is_string()) {
    const auto text = it->get<std::string>();
    std::int64_t value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    if (std::from_chars(begin, end, value).ec == std::errc{}) {
      return value;
    }
  }
  return 0;
}

int json_int(const nlohmann::json& object, std::string_view key) {
  return static_cast<int>(json_int64(object, key));
}

std::vector<std::string> split_tag_string(std::string_view tags) {
  return split_words(tags);
}

}  // namespace boorubox::providers
