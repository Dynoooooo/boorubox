#pragma once

#include <filesystem>
#include <string>

#include <QPalette>
#include <QString>

#include "model/DownloadJob.hpp"
#include "model/LocalArchiveItem.hpp"
#include "model/Post.hpp"
#include "model/Rating.hpp"

namespace boorubox::gui {

QString qs(std::string_view value);
QString qs(const std::string& value);
QString qs(const char* value);
QString qs(const std::filesystem::path& path);
std::string to_std_string(const QString& value);
QString rating_label(Rating rating);
Rating rating_from_label(const QString& value);
QString download_status_label(DownloadStatus status);
QString post_title(const Post& post);
QString archive_title(const LocalArchiveItem& item);
QString bytes_label(std::int64_t bytes);
QString speed_label(double bytes_per_second);
QString preview_panel_style(const QPalette& palette, bool prominent);

}  // namespace boorubox::gui
