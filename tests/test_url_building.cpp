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

TEST_CASE("Danbooru SFW mode never drops the safe rating filter") {
  // Even when the user supplies as many tags as the public tag budget, the
  // safe rating filter must remain in the query so the server cannot return
  // non-safe content. Lower-priority terms (user tags, exclusions) are
  // truncated instead.
  DanbooruProvider provider("https://danbooru.donmai.us");
  SearchQuery query;
  query.tags = {"landscape", "sky"};
  query.excluded_tags = {"watermark"};
  SearchSafety safety;
  safety.enable_nsfw = false;
  safety.blacklisted_tags = {"gore"};

  const auto request = provider.build_search_request(query, safety);

  REQUIRE(request.url.find("rating%3Ag") != std::string::npos);
  // The first user tag still fits under the remaining budget.
  REQUIRE(request.url.find("landscape") != std::string::npos);
}

TEST_CASE("Danbooru NSFW mode preserves user-supplied tags in the budget") {
  // When NSFW mode is on and no explicit rating is requested, the budget is
  // spent entirely on user tags.
  DanbooruProvider provider("https://danbooru.donmai.us");
  SearchQuery query;
  query.tags = {"landscape", "sky"};
  query.rating_filter = Rating::Unknown;
  SearchSafety safety;
  safety.enable_nsfw = true;

  const auto request = provider.build_search_request(query, safety);

  REQUIRE(request.url.find("landscape%20sky") != std::string::npos);
  REQUIRE(request.url.find("rating%3Ag") == std::string::npos);
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
