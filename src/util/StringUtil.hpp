#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace boorubox {

std::string trim(std::string_view value);
std::string lower(std::string_view value);
std::vector<std::string> split_words(std::string_view value);
std::vector<std::string> split(std::string_view value, char delimiter);
std::string join(const std::vector<std::string>& values, std::string_view sep);
std::string url_encode(std::string_view value);
std::string sanitize_filename_component(std::string_view value,
                                        std::size_t max_length = 120);
std::string replace_all(std::string value, std::string_view needle,
                        std::string_view replacement);
bool starts_with(std::string_view value, std::string_view prefix);
std::string redact_url_secrets(std::string_view value);

// True when `needle` is empty OR any element of `values` contains `needle` as
// a case-insensitive substring. Useful for tag/artist filters that should not
// care about provider-specific casing.
bool contains_substring_ci(const std::vector<std::string>& values,
                           std::string_view needle);

}  // namespace boorubox
