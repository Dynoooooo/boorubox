#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <unistd.h>

#include "index/JsonlArchiveIndex.hpp"

using namespace boorubox;

TEST_CASE("JSONL archive indexes and detects duplicates") {
  const auto dir = std::filesystem::temp_directory_path() /
                   ("boorubox_test_" + std::to_string(::getpid()));
  std::filesystem::create_directories(dir);
  const auto index_path = dir / "index.jsonl";

  JsonlArchiveIndex index(index_path);

  LocalArchiveItem item;
  item.provider = "danbooru";
  item.post_id = "1";
  item.hash = "abc";
  item.file_url = "https://cdn.example/a.png";
  item.local_file_path = dir / "a.png";
  item.tags = {"landscape", "sky"};
  item.artists = {"artist"};
  item.rating = Rating::Safe;
  index.upsert(item);

  REQUIRE(index.contains_duplicate("abc", "", "", ""));
  REQUIRE(index.contains_duplicate("", "danbooru", "1", ""));
  REQUIRE(index.contains_duplicate("", "", "", "https://cdn.example/a.png"));
  REQUIRE(index.find_by_provider_id("danbooru", "1").has_value());
  REQUIRE(index.find_by_hash("abc").has_value());
  REQUIRE(index.list(ArchiveQuery{.tag = "land"}).size() == 1);
  REQUIRE(index.list(ArchiveQuery{.artist = "artist"}).size() == 1);

  LocalArchiveItem replacement = item;
  replacement.local_file_path = dir / "b.png";
  index.upsert(replacement);
  REQUIRE(index.list().size() == 1);

  std::filesystem::remove_all(dir);
}
