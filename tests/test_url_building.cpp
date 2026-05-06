#include <catch2/catch_test_macros.hpp>

#include "providers/DanbooruProvider.hpp"
#include "providers/E621Provider.hpp"
#include "providers/GelbooruProvider.hpp"

using namespace boorubox;

TEST_CASE("Danbooru search URL includes safe rating when tag budget allows") {
  DanbooruProvider provider("https://danbooru.donmai.us");
  SearchQuery query;
  query.tags = {"landscape"};
  query.limit = 25;
  query.page = 2;
  SearchSafety safety;
  safety.enable_nsfw = false;
  safety.blacklisted_tags = {"gore"};

  const auto request = provider.build_search_request(query, safety);

  REQUIRE(request.url.find("/posts.json?") != std::string::npos);
  REQUIRE(request.url.find("limit=25") != std::string::npos);
  REQUIRE(request.url.find("page=2") != std::string::npos);
  REQUIRE(request.url.find("rating%3Ag") != std::string::npos);
}

TEST_CASE("Danbooru search URL respects public tag budget") {
  DanbooruProvider provider("https://danbooru.donmai.us");
  SearchQuery query;
  query.tags = {"landscape", "sky"};
  query.excluded_tags = {"watermark"};
  SearchSafety safety;
  safety.enable_nsfw = false;
  safety.blacklisted_tags = {"gore"};

  const auto request = provider.build_search_request(query, safety);

  REQUIRE(request.url.find("landscape%20sky") != std::string::npos);
  REQUIRE(request.url.find("rating%3Ag") == std::string::npos);
  REQUIRE(request.url.find("-watermark") == std::string::npos);
  REQUIRE(request.url.find("-gore") == std::string::npos);
}

TEST_CASE("Gelbooru DAPI URL uses JSON endpoint and safe rating") {
  GelbooruProvider provider("gelbooru", "https://gelbooru.com", false, 100);
  SearchQuery query;
  query.tags = {"cat"};
  query.limit = 101;
  query.page = 1;
  SearchSafety safety;
  safety.enable_nsfw = false;

  const auto request = provider.build_search_request(query, safety);

  REQUIRE(request.url.find("/index.php?") != std::string::npos);
  REQUIRE(request.url.find("page=dapi") != std::string::npos);
  REQUIRE(request.url.find("s=post") != std::string::npos);
  REQUIRE(request.url.find("q=index") != std::string::npos);
  REQUIRE(request.url.find("json=1") != std::string::npos);
  REQUIRE(request.url.find("limit=100") != std::string::npos);
  REQUIRE(request.url.find("pid=0") != std::string::npos);
  REQUIRE(request.url.find("rating%3Ageneral") != std::string::npos);
}

TEST_CASE("e926 search URL uses posts JSON and safe rating") {
  E621Provider provider("e926", "https://e926.net", false);
  SearchQuery query;
  query.tags = {"scenery"};
  query.limit = 5;
  SearchSafety safety;
  safety.enable_nsfw = false;

  const auto request = provider.build_search_request(query, safety);

  REQUIRE(request.url.find("https://e926.net/posts.json?") == 0);
  REQUIRE(request.url.find("limit=5") != std::string::npos);
  REQUIRE(request.url.find("rating%3Asafe") != std::string::npos);
}
