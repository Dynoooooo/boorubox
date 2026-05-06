#pragma once

#include <filesystem>
#include <string>

namespace boorubox {

struct PreviewBounds {
  int x = 0;
  int y = 0;
  int columns = 40;
  int rows = 20;
};

class ImagePreviewBackend {
 public:
  virtual ~ImagePreviewBackend() = default;

  virtual bool is_supported() const = 0;
  virtual void render_image(const std::filesystem::path& path,
                            PreviewBounds bounds) = 0;
  virtual void clear() = 0;
  virtual std::string name() const = 0;
};

}  // namespace boorubox
