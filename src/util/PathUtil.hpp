#pragma once

#include <filesystem>
#include <string_view>

namespace boorubox {

std::filesystem::path expand_user_path(std::string_view value);
void ensure_directory(const std::filesystem::path& path);
std::filesystem::path cache_home();
std::filesystem::path config_home();
std::filesystem::path data_home();
std::filesystem::path state_home();
std::string extension_from_url(std::string_view url);

}  // namespace boorubox
