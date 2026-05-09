#include "gui/GalleryPage.hpp"

#include <algorithm>
#include <set>

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include "gui/QtUtil.hpp"
#include "gui/ThumbnailLoader.hpp"
#include "util/StringUtil.hpp"

namespace boorubox::gui {

namespace {

constexpr QSize kGalleryThumbSize(140, 140);

QString dimensions(const LocalArchiveItem& item) {
  if (item.width <= 0 || item.height <= 0) {
    return "? x ?";
  }
  return QString("%1 x %2").arg(item.width).arg(item.height);
}

bool contains_tag(const std::vector<std::string>& values, const QString& needle) {
  return contains_substring_ci(values, to_std_string(needle.trimmed()));
}

}  // namespace

GalleryPage::GalleryPage(App& app, QWidget* parent) : QWidget(parent), app_(app) {
  tag_filter_ = new QLineEdit(this);
  tag_filter_->setPlaceholderText("Search local tags");

  provider_filter_ = new QComboBox(this);
  rating_filter_ = new QComboBox(this);
  rating_filter_->addItems({"all", "safe", "general", "sensitive",
                            "questionable", "explicit", "unknown"});

  favorites_filter_ = new QCheckBox("Favorites", this);

  sort_box_ = new QComboBox(this);
  sort_box_->addItems({"Downloaded date", "Provider", "Rating", "File size"});

  refresh_button_ = new QPushButton("Refresh", this);
  rebuild_button_ = new QPushButton("Rebuild Index", this);
  open_file_button_ = new QPushButton("Open File", this);
  use_reference_button_ = new QPushButton("Use as Reference", this);
  count_label_ = new QLabel(this);

  list_ = new QListWidget(this);
  list_->setViewMode(QListView::IconMode);
  list_->setResizeMode(QListView::Adjust);
  list_->setMovement(QListView::Static);
  list_->setIconSize(kGalleryThumbSize);
  list_->setGridSize(QSize(190, 205));
  list_->setSpacing(8);

  preview_ = new ImagePreviewWidget(this);
  metadata_ = new QTextEdit(this);
  metadata_->setReadOnly(true);
  metadata_->setStyleSheet(
      QString("QTextEdit { %1 }").arg(preview_panel_style(metadata_->palette(), true)));

  auto* toolbar = new QHBoxLayout;
  toolbar->addWidget(new QLabel("Tags", this));
  toolbar->addWidget(tag_filter_, 2);
  toolbar->addWidget(new QLabel("Provider", this));
  toolbar->addWidget(provider_filter_);
  toolbar->addWidget(new QLabel("Rating", this));
  toolbar->addWidget(rating_filter_);
  toolbar->addWidget(favorites_filter_);
  toolbar->addWidget(new QLabel("Sort", this));
  toolbar->addWidget(sort_box_);
  toolbar->addWidget(refresh_button_);
  toolbar->addWidget(rebuild_button_);

  auto* side = new QWidget(this);
  auto* side_layout = new QVBoxLayout(side);
  side_layout->setContentsMargins(0, 0, 0, 0);
  side_layout->addWidget(preview_, 2);
  side_layout->addWidget(metadata_, 1);
  side_layout->addWidget(use_reference_button_);
  side_layout->addWidget(open_file_button_);

  auto* splitter = new QSplitter(this);
  splitter->addWidget(list_);
  splitter->addWidget(side);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 14, 14, 14);
  layout->setSpacing(10);
  layout->addLayout(toolbar);
  layout->addWidget(count_label_);
  layout->addWidget(splitter, 1);

  refresh_watcher_ = new QFutureWatcher<GalleryWorkerResult>(this);
  connect(refresh_watcher_, &QFutureWatcher<GalleryWorkerResult>::finished, this,
          [this] {
            const auto result = refresh_watcher_->result();
            rebuild_button_->setEnabled(true);
            refresh_button_->setEnabled(true);
            if (!result.error.empty()) {
              if (status_callback_) {
                status_callback_(qs(result.error));
              }
              return;
            }
            all_items_ = result.items;
            std::set<QString> providers;
            for (const auto& item : all_items_) {
              providers.insert(qs(item.provider));
            }
            const auto current_provider = provider_filter_->currentText();
            provider_filter_->blockSignals(true);
            provider_filter_->clear();
            provider_filter_->addItem("all");
            for (const auto& provider : providers) {
              provider_filter_->addItem(provider);
            }
            provider_filter_->setCurrentText(current_provider.isEmpty() ? "all"
                                                                        : current_provider);
            provider_filter_->blockSignals(false);
            applyFilters();
            if (status_callback_) {
              status_callback_(QString("Loaded %1 archive item(s)")
                                   .arg(static_cast<int>(all_items_.size())));
            }
          });

  const auto apply = [this] { applyFilters(); };
  connect(tag_filter_, &QLineEdit::textChanged, this, apply);
  connect(provider_filter_, &QComboBox::currentTextChanged, this, apply);
  connect(rating_filter_, &QComboBox::currentTextChanged, this, apply);
  connect(favorites_filter_, &QCheckBox::toggled, this, apply);
  connect(sort_box_, &QComboBox::currentTextChanged, this, apply);
  connect(refresh_button_, &QPushButton::clicked, this, [this] { refresh(); });
  connect(rebuild_button_, &QPushButton::clicked, this, [this] { rebuildIndex(); });
  connect(open_file_button_, &QPushButton::clicked, this,
          [this] { openSelectedFile(); });
  connect(use_reference_button_, &QPushButton::clicked, this,
          [this] { useSelectedAsReference(); });
  connect(list_, &QListWidget::currentRowChanged, this,
          [this] { showSelectedItem(); });

  preview_->setMessage("Select a local image to preview.");
  refresh();
}

void GalleryPage::refresh() {
  if (refresh_watcher_->isRunning()) {
    return;
  }
  refresh_button_->setEnabled(false);
  refresh_watcher_->setFuture(QtConcurrent::run([this] {
    GalleryWorkerResult result;
    try {
      result.items = app_.archive_items();
    } catch (const std::exception& error) {
      result.error = error.what();
    }
    return result;
  }));
}

void GalleryPage::setStatusCallback(std::function<void(QString)> callback) {
  status_callback_ = std::move(callback);
}

void GalleryPage::setReferenceCallback(
    std::function<void(LocalArchiveItem)> callback) {
  reference_callback_ = std::move(callback);
}

void GalleryPage::applyFilters() {
  visible_items_.clear();
  const auto provider = provider_filter_->currentText();
  const auto rating = rating_filter_->currentText();
  for (const auto& item : all_items_) {
    if (provider != "all" && qs(item.provider) != provider) {
      continue;
    }
    if (rating != "all" && rating_label(item.rating) != rating) {
      continue;
    }
    if (favorites_filter_->isChecked() && !item.favorite) {
      continue;
    }
    if (!contains_tag(item.tags, tag_filter_->text()) &&
        !contains_tag(item.artists, tag_filter_->text())) {
      continue;
    }
    visible_items_.push_back(item);
  }

  const auto sort = sort_box_->currentText();
  std::ranges::sort(visible_items_, [&](const auto& a, const auto& b) {
    if (sort == "Provider") {
      return a.provider < b.provider;
    }
    if (sort == "Rating") {
      return to_string(a.rating) < to_string(b.rating);
    }
    if (sort == "File size") {
      return a.file_size > b.file_size;
    }
    return a.downloaded_at > b.downloaded_at;
  });

  // Bump the generation first so any in-flight thumbnail workers from the
  // previous filter set discard their results on completion.
  ++thumbnail_generation_;

  list_->clear();
  for (std::size_t i = 0; i < visible_items_.size(); ++i) {
    auto* row = new QListWidgetItem(placeholder_icon(kGalleryThumbSize),
                                    archive_title(visible_items_[i]));
    row->setData(Qt::UserRole, static_cast<int>(i));
    list_->addItem(row);
  }
  count_label_->setText(QString("%1 item(s)").arg(static_cast<int>(visible_items_.size())));
  use_reference_button_->setEnabled(!visible_items_.empty());
  if (!visible_items_.empty()) {
    list_->setCurrentRow(0);
    loadThumbnails();
  } else {
    preview_->setMessage("No local gallery items match the current filters.");
    metadata_->clear();
  }
}

void GalleryPage::loadThumbnails() {
  const int generation = thumbnail_generation_;
  for (std::size_t index = 0; index < visible_items_.size(); ++index) {
    const auto path = visible_items_[index].local_file_path;
    const int row = static_cast<int>(index);
    auto* watcher = new QFutureWatcher<GalleryThumbnailResult>(this);
    connect(watcher, &QFutureWatcher<GalleryThumbnailResult>::finished, this,
            [this, watcher] {
              const auto result = watcher->result();
              watcher->deleteLater();
              if (result.generation != thumbnail_generation_ ||
                  result.row < 0 || result.row >= list_->count() ||
                  result.image.isNull()) {
                return;
              }
              list_->item(result.row)->setIcon(
                  QIcon(QPixmap::fromImage(result.image)));
            });
    watcher->setFuture(QtConcurrent::run([row, generation, path] {
      GalleryThumbnailResult result;
      result.row = row;
      result.generation = generation;
      result.image = load_scaled_image(path, kGalleryThumbSize);
      return result;
    }));
  }
}

void GalleryPage::rebuildIndex() {
  if (refresh_watcher_->isRunning()) {
    return;
  }
  rebuild_button_->setEnabled(false);
  refresh_button_->setEnabled(false);
  if (status_callback_) {
    status_callback_("Rebuilding archive index...");
  }
  refresh_watcher_->setFuture(QtConcurrent::run([this] {
    GalleryWorkerResult result;
    try {
      app_.rebuild_archive();
      result.items = app_.archive_items();
    } catch (const std::exception& error) {
      result.error = error.what();
    }
    return result;
  }));
}

void GalleryPage::showSelectedItem() {
  const auto row = list_->currentRow();
  if (row < 0 || row >= static_cast<int>(visible_items_.size())) {
    preview_->setMessage("Select a local image to preview.");
    metadata_->clear();
    return;
  }
  const auto& item = visible_items_[row];
  preview_->setImagePath(item.local_file_path);
  metadata_->setPlainText(metadataText(item));
}

void GalleryPage::openSelectedFile() {
  const auto row = list_->currentRow();
  if (row < 0 || row >= static_cast<int>(visible_items_.size())) {
    return;
  }
  QDesktopServices::openUrl(
      QUrl::fromLocalFile(qs(visible_items_[row].local_file_path)));
}

void GalleryPage::useSelectedAsReference() {
  const auto row = list_->currentRow();
  if (row < 0 || row >= static_cast<int>(visible_items_.size())) {
    if (status_callback_) {
      status_callback_("No gallery item selected");
    }
    return;
  }
  if (reference_callback_) {
    reference_callback_(visible_items_[row]);
  }
}

QString GalleryPage::metadataText(const LocalArchiveItem& item) const {
  QString text;
  text += "Provider: " + qs(item.provider) + "\n";
  text += "Post ID: " + qs(item.post_id) + "\n";
  text += "Rating: " + rating_label(item.rating) + "\n";
  text += "Size: " + dimensions(item) + "\n";
  text += "File size: " + bytes_label(item.file_size) + "\n";
  text += "Downloaded: " + qs(item.downloaded_at) + "\n";
  text += "Artists: " + qs(join(item.artists, " ")) + "\n";
  text += "Tags: " + qs(join(item.tags, " ")) + "\n";
  text += "Local path: " + qs(item.local_file_path) + "\n";
  if (!item.post_url.empty()) {
    text += "Post URL: " + qs(item.post_url) + "\n";
  }
  return text;
}

}  // namespace boorubox::gui
