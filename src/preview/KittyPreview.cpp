#include "preview/KittyPreview.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace boorubox {

namespace {

std::string base64_encode(std::string_view input) {
  static constexpr char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((input.size() + 2) / 3) * 4);
  for (std::size_t i = 0; i < input.size(); i += 3) {
    const auto b0 = static_cast<unsigned char>(input[i]);
    const auto b1 =
        i + 1 < input.size() ? static_cast<unsigned char>(input[i + 1]) : 0;
    const auto b2 =
        i + 2 < input.size() ? static_cast<unsigned char>(input[i + 2]) : 0;

    out.push_back(alphabet[b0 >> 2]);
    out.push_back(alphabet[((b0 & 0x03) << 4) | (b1 >> 4)]);
    out.push_back(i + 1 < input.size()
                      ? alphabet[((b1 & 0x0f) << 2) | (b2 >> 6)]
                      : '=');
    out.push_back(i + 2 < input.size() ? alphabet[b2 & 0x3f] : '=');
  }
  return out;
}

bool env_contains(const char* name, std::string_view needle) {
  const char* value = std::getenv(name);
  return value != nullptr && std::string_view(value).find(needle) != std::string_view::npos;
}

}  // namespace

bool KittyPreview::is_supported() const {
  return std::getenv("KITTY_WINDOW_ID") != nullptr ||
         env_contains("TERM", "xterm-kitty") ||
         env_contains("TERM", "wezterm");
}

void KittyPreview::render_image(const std::filesystem::path& path,
                                PreviewBounds bounds) {
  if (!is_supported()) {
    return;
  }
  const auto encoded_path = base64_encode(std::filesystem::absolute(path).string());
  std::cout << "\033_Ga=T,t=f,c=" << bounds.columns << ",r=" << bounds.rows << ';'
            << encoded_path << "\033\\" << std::flush;
}

void KittyPreview::clear() {
  if (is_supported()) {
    std::cout << "\033_Ga=d\033\\" << std::flush;
  }
}

std::string KittyPreview::name() const {
  return "kitty";
}

}  // namespace boorubox
