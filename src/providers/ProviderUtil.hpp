#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "model/SearchQuery.hpp"
#include "providers/SiteProvider.hpp"

namespace boorubox::providers {

std::string append_query_params(
    std::string base,
    const std::vector<std::pair<std::string, std::string>>& params);
std::string without_trailing_slash(std::string value);
std::string absolutize_url(std::string_view base_url, std::string_view value);
std::vector<std::string> tag_terms_with_exclusions(
    const SearchQuery& query, const SearchSafety& safety);
std::string joined_tag_terms(const std::vector<std::string>& terms);
int clamp_limit(int value, int max_limit);

std::string json_string(const nlohmann::json& object, std::string_view key);
std::int64_t json_int64(const nlohmann::json& object, std::string_view key);
int json_int(const nlohmann::json& object, std::string_view key);
std::vector<std::string> split_tag_string(std::string_view tags);

}  // namespace boorubox::providers
