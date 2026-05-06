#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace boorubox {

enum class Rating {
  Safe,
  General,
  Sensitive,
  Questionable,
  Explicit,
  Unknown,
};

inline std::string normalized_rating_token(std::string_view value) {
  std::string out(value);
  std::ranges::transform(out, out.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return out;
}

inline Rating rating_from_string(std::string_view value) {
  const auto token = normalized_rating_token(value);
  if (token == "s" || token == "safe") {
    return Rating::Safe;
  }
  if (token == "g" || token == "general") {
    return Rating::General;
  }
  if (token == "sensitive") {
    return Rating::Sensitive;
  }
  if (token == "q" || token == "questionable") {
    return Rating::Questionable;
  }
  if (token == "e" || token == "explicit") {
    return Rating::Explicit;
  }
  return Rating::Unknown;
}

inline std::string to_string(Rating rating) {
  switch (rating) {
    case Rating::Safe:
      return "safe";
    case Rating::General:
      return "general";
    case Rating::Sensitive:
      return "sensitive";
    case Rating::Questionable:
      return "questionable";
    case Rating::Explicit:
      return "explicit";
    case Rating::Unknown:
      return "unknown";
  }
  return "unknown";
}

inline bool is_allowed_in_sfw_mode(Rating rating) {
  return rating == Rating::Safe || rating == Rating::General ||
         rating == Rating::Unknown;
}

}  // namespace boorubox
