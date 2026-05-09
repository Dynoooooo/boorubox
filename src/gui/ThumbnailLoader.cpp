#include "gui/ThumbnailLoader.hpp"

#include <QPainter>

#include "gui/QtUtil.hpp"

namespace boorubox::gui {

QPixmap load_pixmap(const std::filesystem::path& path, QSize size) {
  QPixmap pixmap(qs(path));
  if (pixmap.isNull()) {
    return {};
  }
  return pixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QImage load_scaled_image(const std::filesystem::path& path, QSize size) {
  QImage image(qs(path));
  if (image.isNull()) {
    return {};
  }
  return image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QIcon placeholder_icon(QSize size) {
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(QPen(QColor(120, 120, 120), 1));
  painter.setBrush(QColor(230, 230, 230));
  painter.drawRoundedRect(pixmap.rect().adjusted(2, 2, -2, -2), 6, 6);
  painter.drawLine(12, size.height() - 16, size.width() / 2, size.height() / 2);
  painter.drawLine(size.width() / 2, size.height() / 2, size.width() - 12,
                   size.height() - 18);
  return QIcon(pixmap);
}

}  // namespace boorubox::gui
