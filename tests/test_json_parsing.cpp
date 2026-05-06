#include <catch2/catch_test_macros.hpp>

#include "providers/DanbooruProvider.hpp"
#include "providers/E621Provider.hpp"
#include "providers/GelbooruProvider.hpp"

using namespace boorubox;

TEST_CASE("Danbooru JSON normalizes post fields") {
  DanbooruProvider provider("https://danbooru.donmai.us");
  const auto posts = provider.parse_search_response(R"([
    {
      "id": 42,
      "rating": "g",
      "file_url": "https://cdn.example/file.png",
      "large_file_url": "https://cdn.example/sample.jpg",
      "preview_file_url": "https://cdn.example/preview.jpg",
      "md5": "abc",
      "image_width": 800,
      "image_height": 600,
      "file_ext": "png",
      "file_size": 1234,
      "score": 9,
      "fav_count": 2,
      "tag_string": "landscape sky",
      "tag_string_artist": "artist_name",
      "source": "https://source.example",
      "created_at": "2026-01-02T03:04:05Z"
    }
  ])");

  REQUIRE(posts.size() == 1);
  REQUIRE(posts[0].provider == "danbooru");
  REQUIRE(posts[0].id == "42");
  REQUIRE(posts[0].rating == Rating::General);
  REQUIRE(posts[0].file_url == "https://cdn.example/file.png");
  REQUIRE(posts[0].artist_tags == std::vector<std::string>{"artist_name"});
}

TEST_CASE("Gelbooru JSON object with post array is normalized") {
  GelbooruProvider provider("gelbooru", "https://gelbooru.com", false, 100);
  const auto posts = provider.parse_search_response(R"({
    "post": [
      {
        "id": 7,
        "rating": "general",
        "file_url": "/images/file.jpg",
        "preview_url": "/thumbnails/preview.jpg",
        "md5": "def",
        "width": 640,
        "height": 480,
        "tags": "cat cute",
        "score": 3
      }
    ]
  })");

  REQUIRE(posts.size() == 1);
  REQUIRE(posts[0].id == "7");
  REQUIRE(posts[0].rating == Rating::General);
  REQUIRE(posts[0].file_url == "https://gelbooru.com/images/file.jpg");
  REQUIRE(posts[0].tags == std::vector<std::string>{"cat", "cute"});
}

TEST_CASE("e926 JSON normalizes nested media and tags") {
  E621Provider provider("e926", "https://e926.net", false);
  const auto posts = provider.parse_search_response(R"({
    "posts": [
      {
        "id": 99,
        "rating": "s",
        "file": {"width": 100, "height": 200, "ext": "webp", "size": 50, "md5": "ghi", "url": "https://static.example/file.webp"},
        "preview": {"url": "https://static.example/preview.jpg"},
        "sample": {"url": "https://static.example/sample.jpg"},
        "score": {"total": 12},
        "fav_count": 4,
        "tags": {"general": ["tree"], "artist": ["artist_two"], "meta": ["hi_res"]},
        "sources": ["https://source.example"]
      }
    ]
  })");

  REQUIRE(posts.size() == 1);
  REQUIRE(posts[0].provider == "e926");
  REQUIRE(posts[0].id == "99");
  REQUIRE(posts[0].rating == Rating::Safe);
  REQUIRE(posts[0].artist_tags == std::vector<std::string>{"artist_two"});
  REQUIRE(posts[0].tags.size() == 3);
}
