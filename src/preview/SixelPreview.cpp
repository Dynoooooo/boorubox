#include "preview/SixelPreview.hpp"

#include <cstdlib>
#include <iostream>

namespace boorubox {

bool SixelPreview::is_supported() const {
  const char* term = std::getenv("TERM");
  return term != nullptr && std::string(term).find("sixel") != std::string::npos;
}

void SixelPreview::render_image(const std::filesystem::path& path,
                                PreviewBounds) {
  if (is_supported()) {
    std::cout << "[sixel preview backend is detected but encoder is not linked: "
              << path.string() << "]" << std::flush;
  }
}

void SixelPreview::clear() {}

std::string SixelPreview::name() const {
  return "sixel";
}

}  // namespace boorubox
