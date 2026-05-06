#pragma once

#include <filesystem>

#include <QIcon>
#include <QPixmap>
#include <QSize>

namespace boorubox::gui {

QPixmap load_pixmap(const std::filesystem::path& path, QSize size);
QIcon placeholder_icon(QSize size);

}  // namespace boorubox::gui
