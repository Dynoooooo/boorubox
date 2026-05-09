#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "http/HttpClient.hpp"

using namespace boorubox;

namespace {

std::filesystem::path temp_http_root(std::string_view name) {
  const auto dir = std::filesystem::temp_directory_path() /
                   ("boorubox_http_test_" + std::to_string(::getpid()) + "_" +
                    std::string(name));
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

void write_file(const std::filesystem::path& path, std::string_view body) {
  std::ofstream out(path, std::ios::trunc | std::ios::binary);
  out.write(body.data(), static_cast<std::streamsize>(body.size()));
}

std::string file_url(const std::filesystem::path& path) {
  // curl's file:// scheme accepts absolute paths. Our test files live in
  // /tmp/boorubox_* which has no special characters to escape.
  return "file://" + std::filesystem::absolute(path).string();
}

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::string contents((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  return contents;
}

}  // namespace

TEST_CASE("HttpClient::get reads the full body from a file:// URL") {
  const auto dir = temp_http_root("get_basic");
  const auto source = dir / "body.txt";
  write_file(source, "hello boorubox");

  HttpClient client("BooruBoxTest/0.0");
  const auto response = client.get(file_url(source));

  REQUIRE(response.body == "hello boorubox");

  std::error_code ignored;
  std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("HttpClient::download_to_file writes the full body to disk") {
  const auto dir = temp_http_root("dl_basic");
  const auto source = dir / "source.bin";
  const auto destination = dir / "out.bin";
  write_file(source, "abcdef0123456789");

  HttpClient client("BooruBoxTest/0.0");
  client.download_to_file(file_url(source), destination, /*resume=*/false);

  REQUIRE(std::filesystem::exists(destination));
  REQUIRE(read_file(destination) == "abcdef0123456789");

  std::error_code ignored;
  std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("HttpClient::download_to_file honours should_cancel callback") {
  const auto dir = temp_http_root("dl_cancel");
  const auto source = dir / "source.bin";
  const auto destination = dir / "out.bin";
  write_file(source, std::string(4096, 'x'));

  HttpClient client("BooruBoxTest/0.0");
  std::atomic<bool> cancelled{true};
  REQUIRE_THROWS_AS(
      client.download_to_file(
          file_url(source), destination, /*resume=*/false,
          /*progress=*/{},
          /*should_cancel=*/[&] { return cancelled.load(); }),
      HttpRequestCancelled);

  std::error_code ignored;
  std::filesystem::remove_all(dir, ignored);
}

TEST_CASE(
    "HttpClient::download_to_file resumes correctly when the server honours "
    "Range") {
  // curl's file:// implementation honours Range requests the same way a
  // compliant HTTP server does: it returns only the requested byte range,
  // which our code appends to the pre-existing partial file. This protects
  // the normal "happy path" of partial download resumption.
  const auto dir = temp_http_root("dl_resume_honored");
  const auto source = dir / "source.bin";
  const auto destination = dir / "out.bin";
  write_file(source, "0123456789ABCDEF");
  // Pretend the first 10 bytes already downloaded successfully.
  write_file(destination, "0123456789");

  HttpClient client("BooruBoxTest/0.0");
  client.download_to_file(file_url(source), destination, /*resume=*/true);

  REQUIRE(read_file(destination) == "0123456789ABCDEF");

  std::error_code ignored;
  std::filesystem::remove_all(dir, ignored);
}
