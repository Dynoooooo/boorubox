#pragma once

#include <filesystem>
#include <string>

#include "model/Post.hpp"

namespace boorubox {

class FilenameTemplate {
 public:
  explicit FilenameTemplate(std::string pattern);

  std::filesystem::path render_relative(const Post& post) const;
  std::filesystem::path render_under(const std::filesystem::path& root,
                                     const Post& post) const;

 private:
  std::string pattern_;
};

}  // namespace boorubox
