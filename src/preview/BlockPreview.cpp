#include "preview/BlockPreview.hpp"

#include <iostream>

namespace boorubox {

bool BlockPreview::is_supported() const {
  return true;
}

void BlockPreview::render_image(const std::filesystem::path& path,
                                PreviewBounds) {
  std::cout << "[preview unavailable: " << path.string() << "]" << std::flush;
}

void BlockPreview::clear() {}

std::string BlockPreview::name() const {
  return "block";
}

}  // namespace boorubox
