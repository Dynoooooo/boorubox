#include "gui/ImagePreviewWidget.hpp"

#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "gui/QtUtil.hpp"

namespace boorubox::gui {

ImagePreviewWidget::ImagePreviewWidget(QWidget* parent) : QWidget(parent) {
  image_label_ = new QLabel(this);
  image_label_->setAlignment(Qt::AlignCenter);
  image_label_->setMinimumSize(240, 180);
  image_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  applyFrameStyle(true);

  message_label_ = new QLabel("No preview selected", this);
  message_label_->setAlignment(Qt::AlignCenter);
  message_label_->setWordWrap(true);
  message_label_->setFixedHeight(44);
  message_label_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  message_label_->setStyleSheet(
      "QLabel { color: palette(text); font-size: 15px; font-weight: 500; }");

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);
  layout->addWidget(image_label_, 1);
  layout->addWidget(message_label_);
}

void ImagePreviewWidget::setImagePath(const std::filesystem::path& path) {
  pixmap_ = QPixmap(qs(path));
  if (pixmap_.isNull()) {
    setMessage("Preview image could not be loaded.");
    image_label_->clear();
    return;
  }
  applyFrameStyle(false);
  message_label_->setText(qs(path.filename().string()));
  updatePixmap();
}

void ImagePreviewWidget::setMessage(const QString& message) {
  pixmap_ = {};
  image_label_->clear();
  applyFrameStyle(true);
  message_label_->setText(message);
}

void ImagePreviewWidget::clear() {
  setMessage("No preview selected");
}

void ImagePreviewWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  updatePixmap();
}

void ImagePreviewWidget::applyFrameStyle(bool empty_state) {
  image_label_->setStyleSheet(
      QString("QLabel { %1 }").arg(preview_panel_style(palette(), empty_state)));
}

void ImagePreviewWidget::updatePixmap() {
  if (pixmap_.isNull()) {
    return;
  }
  image_label_->setPixmap(pixmap_.scaled(image_label_->size(), Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
}

}  // namespace boorubox::gui
