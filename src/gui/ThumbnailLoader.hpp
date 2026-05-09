#pragma once

#include <filesystem>

#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QSize>

namespace boorubox::gui {

QPixmap load_pixmap(const std::filesystem::path& path, QSize size);

// Thread-safe variant: loads and scales a QImage. Can be called from any
// thread. Convert the returned QImage to QPixmap on the GUI thread before
// display.
QImage load_scaled_image(const std::filesystem::path& path, QSize size);

QIcon placeholder_icon(QSize size);

}  // namespace boorubox::gui
