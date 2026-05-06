#include "gui/QtUtil.hpp"

#include <array>

#include <QColor>
#include <QPalette>

namespace boorubox::gui {
namespace {

QString css_color(const QColor& color, int alpha = 255) {
  return QString("rgba(%1, %2, %3, %4)")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue())
      .arg(alpha);
}

}  // namespace

QString qs(std::string_view value) {
  return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QString qs(const std::string& value) {
  return QString::fromStdString(value);
}

QString qs(const char* value) {
  return QString::fromUtf8(value == nullptr ? "" : value);
}

QString qs(const std::filesystem::path& path) {
  return QString::fromStdString(path.string());
}

std::string to_std_string(const QString& value) {
  return value.toStdString();
}

QString rating_label(Rating rating) {
  return qs(to_string(rating));
}

Rating rating_from_label(const QString& value) {
  return rating_from_string(to_std_string(value));
}

QString download_status_label(DownloadStatus status) {
  return qs(to_string(status));
}

QString post_title(const Post& post) {
  return QString("%1 / %2").arg(qs(post.provider), qs(post.id));
}

QString archive_title(const LocalArchiveItem& item) {
  return QString("%1 / %2").arg(qs(item.provider), qs(item.post_id));
}

QString bytes_label(std::int64_t bytes) {
  constexpr std::array<const char*, 5> units = {"B", "KiB", "MiB", "GiB", "TiB"};
  double value = static_cast<double>(bytes);
  std::size_t unit = 0;
  while (value >= 1024.0 && unit + 1 < units.size()) {
    value /= 1024.0;
    ++unit;
  }
  return QString("%1 %2").arg(value, 0, 'f', unit == 0 ? 0 : 1).arg(units[unit]);
}

QString speed_label(double bytes_per_second) {
  if (bytes_per_second <= 0.0) {
    return {};
  }
  return bytes_label(static_cast<std::int64_t>(bytes_per_second)) + "/s";
}

QString preview_panel_style(const QPalette& palette, bool prominent) {
  const auto background = palette.color(QPalette::Base);
  const auto border = prominent ? palette.color(QPalette::Text)
                                : palette.color(QPalette::Mid);
  constexpr auto border_width = 3;
  const auto border_alpha = prominent ? 150 : 255;

  return QString("background: %1; border: %2px solid %3; border-radius: 0px;")
      .arg(css_color(background))
      .arg(border_width)
      .arg(css_color(border, border_alpha));
}

}  // namespace boorubox::gui
