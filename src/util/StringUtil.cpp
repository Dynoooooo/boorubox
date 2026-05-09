#include "util/StringUtil.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace boorubox {

std::string trim(std::string_view value) {
  auto begin = value.begin();
  auto end = value.end();
  while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
    ++begin;
  }
  while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
    --end;
  }
  return std::string(begin, end);
}

std::string lower(std::string_view value) {
  std::string out(value);
  std::ranges::transform(out, out.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return out;
}

std::vector<std::string> split_words(std::string_view value) {
  std::istringstream stream{std::string(value)};
  std::vector<std::string> out;
  std::string word;
  while (stream >> word) {
    out.push_back(word);
  }
  return out;
}

std::vector<std::string> split(std::string_view value, char delimiter) {
  std::vector<std::string> out;
  std::string current;
  for (char ch : value) {
    if (ch == delimiter) {
      out.push_back(current);
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  out.push_back(current);
  return out;
}

std::string join(const std::vector<std::string>& values, std::string_view sep) {
  std::string out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out += sep;
    }
    out += values[i];
  }
  return out;
}

std::string url_encode(std::string_view value) {
  std::ostringstream encoded;
  encoded << std::uppercase << std::hex;
  for (const unsigned char ch : value) {
    const bool unreserved =
        std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~';
    if (unreserved) {
      encoded << static_cast<char>(ch);
    } else {
      encoded << '%' << std::setw(2) << std::setfill('0')
              << static_cast<int>(ch);
    }
  }
  return encoded.str();
}

std::string sanitize_filename_component(std::string_view value,
                                        std::size_t max_length) {
  std::string out;
  out.reserve(value.size());

  for (const unsigned char ch : value) {
    const bool invalid = ch < 32 || ch == 127 || ch == '/' || ch == '\\' ||
                         ch == ':' || ch == '*' || ch == '?' || ch == '"' ||
                         ch == '<' || ch == '>' || ch == '|';
    if (invalid) {
      out.push_back('_');
    } else {
      out.push_back(static_cast<char>(ch));
    }
  }

  out = trim(out);
  while (!out.empty() && (out.back() == '.' || out.back() == ' ')) {
    out.pop_back();
  }
  while (!out.empty() &&
         (out.front() == '.' || out.front() == ' ' || out.front() == '_')) {
    out.erase(out.begin());
  }
  if (out == "." || out == ".." || out.empty()) {
    out = "unknown";
  }
  if (out.size() > max_length) {
    out.resize(max_length);
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) {
      out.pop_back();
    }
  }
  return out.empty() ? "unknown" : out;
}

std::string replace_all(std::string value, std::string_view needle,
                        std::string_view replacement) {
  if (needle.empty()) {
    return value;
  }
  std::size_t position = 0;
  while ((position = value.find(needle, position)) != std::string::npos) {
    value.replace(position, needle.size(), replacement);
    position += replacement.size();
  }
  return value;
}

bool starts_with(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.substr(0, prefix.size()) == prefix;
}

std::string redact_url_secrets(std::string_view value) {
  std::string out(value);
  const auto query_start = out.find('?');
  if (query_start == std::string::npos) {
    return out;
  }

  const std::unordered_set<std::string> secret_keys = {
      "api_key", "apikey", "key", "token", "access_token", "auth_token",
      "password", "passwd", "pass", "secret", "client_secret", "user_id",
      "userid", "login",
  };

  std::size_t position = query_start + 1;
  while (position < out.size()) {
    const auto pair_end = out.find_first_of("&#", position);
    const auto actual_end = pair_end == std::string::npos ? out.size() : pair_end;
    const auto equals = out.find('=', position);
    if (equals != std::string::npos && equals < actual_end) {
      const auto key = lower(out.substr(position, equals - position));
      if (secret_keys.contains(key)) {
        out.replace(equals + 1, actual_end - equals - 1, "<redacted>");
      }
    }
    if (pair_end == std::string::npos || out[pair_end] == '#') {
      break;
    }
    position = pair_end + 1;
  }
  return out;
}

bool contains_substring_ci(const std::vector<std::string>& values,
                           std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  const auto lowered = lower(needle);
  for (const auto& value : values) {
    if (lower(value).find(lowered) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace boorubox
