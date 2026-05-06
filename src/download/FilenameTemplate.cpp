#include "download/FilenameTemplate.hpp"

#include "util/PathUtil.hpp"
#include "util/StringUtil.hpp"

namespace boorubox {

FilenameTemplate::FilenameTemplate(std::string pattern)
    : pattern_(std::move(pattern)) {}

std::filesystem::path FilenameTemplate::render_relative(const Post& post) const {
  const auto artist =
      post.artist_tags.empty() ? std::string("unknown_artist") : post.artist_tags[0];
  auto ext = post.file_ext.empty() ? extension_from_url(post.file_url) : post.file_ext;
  if (ext.empty()) {
    ext = "bin";
  }

  const auto safe_provider = sanitize_filename_component(post.provider);
  const auto safe_artist = sanitize_filename_component(artist);
  const auto safe_id =
      sanitize_filename_component(post.id.empty() ? "unknown" : post.id);
  const auto safe_hash =
      sanitize_filename_component(post.hash.empty() ? "nohash" : post.hash);
  const auto safe_ext = sanitize_filename_component(ext);

  std::string rendered = pattern_;
  rendered = replace_all(rendered, "{provider}", safe_provider);
  rendered = replace_all(rendered, "{artist}", safe_artist);
  rendered = replace_all(rendered, "{id}", safe_id);
  rendered = replace_all(rendered, "{md5}", safe_hash);
  rendered = replace_all(rendered, "{hash}", safe_hash);
  rendered = replace_all(rendered, "{ext}", safe_ext);

  std::filesystem::path out;
  for (const auto& part : split(rendered, '/')) {
    out /= sanitize_filename_component(part);
  }
  return out;
}

std::filesystem::path FilenameTemplate::render_under(
    const std::filesystem::path& root, const Post& post) const {
  return root / render_relative(post);
}

}  // namespace boorubox
