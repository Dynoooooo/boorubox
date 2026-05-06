#pragma once

#include <filesystem>

#include <QLabel>
#include <QPixmap>
#include <QWidget>

namespace boorubox::gui {

class ImagePreviewWidget final : public QWidget {
 public:
  explicit ImagePreviewWidget(QWidget* parent = nullptr);

  void setImagePath(const std::filesystem::path& path);
  void setMessage(const QString& message);
  void clear();

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private:
  void applyFrameStyle(bool empty_state);
  void updatePixmap();

  QLabel* image_label_ = nullptr;
  QLabel* message_label_ = nullptr;
  QPixmap pixmap_;
};

}  // namespace boorubox::gui
