#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <unistd.h>

#include "index/SqliteArchiveIndex.hpp"

using namespace boorubox;

TEST_CASE("SQLite archive indexes and detects duplicates") {
  const auto dir = std::filesystem::temp_directory_path() /
                   ("boorubox_sqlite_test_" + std::to_string(::getpid()));
  std::filesystem::create_directories(dir);
  const auto index_path = dir / "index.sqlite";

  SqliteArchiveIndex index(index_path);

  LocalArchiveItem item;
  item.provider = "danbooru";
  item.post_id = "10";
  item.hash = "hash10";
  item.file_url = "https://cdn.example/10.png";
  item.local_file_path = dir / "10.png";
  item.tags = {"landscape", "safe"};
  item.artists = {"artist"};
  item.rating = Rating::Safe;
  index.upsert(item);

  REQUIRE(index.contains_duplicate("hash10", "", "", ""));
  REQUIRE(index.contains_duplicate("", "danbooru", "10", ""));
  REQUIRE(index.contains_duplicate("", "", "", "https://cdn.example/10.png"));
  REQUIRE(index.find_by_provider_id("danbooru", "10").has_value());
  REQUIRE(index.find_by_hash("hash10").has_value());
  REQUIRE(index.list(ArchiveQuery{.tag = "land"}).size() == 1);

  LocalArchiveItem replacement = item;
  replacement.local_file_path = dir / "replacement.png";
  index.upsert(replacement);
  REQUIRE(index.list().size() == 1);

  std::filesystem::remove_all(dir);
}
