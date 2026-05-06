#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <unistd.h>

#include "app/Config.hpp"
#include "util/StringUtil.hpp"

using namespace boorubox;

namespace {

std::filesystem::path temp_config_path(std::string_view name) {
  const auto dir = std::filesystem::temp_directory_path() /
                   ("boorubox_config_test_" + std::to_string(::getpid()));
  std::filesystem::create_directories(dir);
  return dir / std::string(name);
}

void write_file(const std::filesystem::path& path, std::string_view body) {
  std::ofstream out(path, std::ios::trunc);
  out << body;
}

}  // namespace

TEST_CASE("config rejects invalid bool values with file context") {
  const auto path = temp_config_path("bad_bool.toml");
  write_file(path, R"(
[app]
enable_nsfw = maybe
)");

  REQUIRE_THROWS_WITH(Config::load(path),
                      Catch::Matchers::ContainsSubstring("bad_bool.toml:3") &&
                          Catch::Matchers::ContainsSubstring("invalid bool"));

  std::filesystem::remove_all(path.parent_path());
}

TEST_CASE("config rejects invalid ratings") {
  const auto path = temp_config_path("bad_rating.toml");
  write_file(path, R"(
[filters]
default_rating = "spicy"
)");

  REQUIRE_THROWS_WITH(Config::load(path),
                      Catch::Matchers::ContainsSubstring("bad_rating.toml:3") &&
                          Catch::Matchers::ContainsSubstring("invalid rating"));

  std::filesystem::remove_all(path.parent_path());
}

TEST_CASE("URL redaction hides query secrets but preserves normal terms") {
  const auto redacted = redact_url_secrets(
      "https://example.test/api?tags=cat&api_key=secret&user_id=123#frag");

  REQUIRE(redacted.find("tags=cat") != std::string::npos);
  REQUIRE(redacted.find("api_key=<redacted>") != std::string::npos);
  REQUIRE(redacted.find("user_id=<redacted>") != std::string::npos);
  REQUIRE(redacted.find("secret") == std::string::npos);
  REQUIRE(redacted.find("123") == std::string::npos);
  REQUIRE(redacted.ends_with("#frag"));
}
