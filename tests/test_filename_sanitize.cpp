#include <catch2/catch_test_macros.hpp>

#include "download/FilenameTemplate.hpp"
#include "util/StringUtil.hpp"

using namespace boorubox;

TEST_CASE("filename components remove traversal and reserved characters") {
  REQUIRE(sanitize_filename_component("../bad:name?.jpg") == "bad_name_.jpg");
  REQUIRE(sanitize_filename_component("..") == "unknown");
  REQUIRE(sanitize_filename_component("  file.  ") == "file");
}

TEST_CASE("filename template sanitizes each path component") {
  Post post;
  post.provider = "safe/booru";
  post.artist_tags = {"artist:name"};
  post.id = "../42";
  post.hash = "abc";
  post.file_ext = "png";

  FilenameTemplate tpl("{provider}/{artist}/{id}_{md5}.{ext}");
  const auto path = tpl.render_relative(post);

  REQUIRE(path.string().find("..") == std::string::npos);
  REQUIRE(path.string().find(':') == std::string::npos);
  REQUIRE(path.string() == "safe_booru/artist_name/42_abc.png");
}
