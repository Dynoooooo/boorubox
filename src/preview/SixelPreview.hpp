#pragma once

#include "preview/ImagePreviewBackend.hpp"

namespace boorubox {

class SixelPreview final : public ImagePreviewBackend {
 public:
  bool is_supported() const override;
  void render_image(const std::filesystem::path& path,
                    PreviewBounds bounds) override;
  void clear() override;
  std::string name() const override;
};

}  // namespace boorubox
