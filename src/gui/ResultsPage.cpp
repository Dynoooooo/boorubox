#include "gui/ResultsPage.hpp"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
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

constexpr QSize kThumbSize(144, 144);

QString dimensions(const Post& post) {
  if (post.width <= 0 || post.height <= 0) {
    return "? x ?";
  }
  return QString("%1 x %2").arg(post.width).arg(post.height);
}

}  // namespace

ResultsPage::ResultsPage(App& app, QWidget* parent) : QWidget(parent), app_(app) {
  list_ = new QListWidget(this);
  list_->setViewMode(QListView::IconMode);
  list_->setResizeMode(QListView::Adjust);
  list_->setMovement(QListView::Static);
  list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  list_->setIconSize(kThumbSize);
  list_->setGridSize(QSize(190, 210));
  list_->setSpacing(8);

  preview_ = new ImagePreviewWidget(this);
  metadata_ = new QTextEdit(this);
  metadata_->setReadOnly(true);
  metadata_->setMinimumWidth(320);

  count_label_ = new QLabel("No results", this);
  use_reference_button_ = new QPushButton("Use as Reference", this);
  download_selected_button_ = new QPushButton("Download Selected", this);
  download_all_button_ = new QPushButton("Download All Visible", this);
  open_url_button_ = new QPushButton("Open Post URL", this);

  auto* toolbar = new QHBoxLayout;
  toolbar->addWidget(count_label_);
  toolbar->addStretch();
  toolbar->addWidget(use_reference_button_);
  toolbar->addWidget(open_url_button_);
  toolbar->addWidget(download_selected_button_);
  toolbar->addWidget(download_all_button_);

  auto* side = new QWidget(this);
  auto* side_layout = new QVBoxLayout(side);
  side_layout->setContentsMargins(0, 0, 0, 0);
  side_layout->setSpacing(10);
  side_layout->addWidget(preview_, 2);
  side_layout->addWidget(metadata_, 1);

  auto* splitter = new QSplitter(this);
  splitter->addWidget(list_);
  splitter->addWidget(side);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 1);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 14, 14, 14);
  layout->setSpacing(10);
  layout->addLayout(toolbar);
  layout->addWidget(splitter, 1);

  connect(list_, &QListWidget::currentRowChanged, this,
          [this] { showSelectedPost(); });
  connect(download_selected_button_, &QPushButton::clicked, this,
          [this] { downloadSelected(); });
  connect(download_all_button_, &QPushButton::clicked, this,
          [this] { downloadAllVisible(); });
  connect(open_url_button_, &QPushButton::clicked, this,
          [this] { openSelectedPostUrl(); });
  connect(use_reference_button_, &QPushButton::clicked, this,
          [this] { useSelectedAsReference(); });
}

void ResultsPage::setPosts(std::vector<Post> posts) {
  ++generation_;
  posts_ = std::move(posts);
  list_->clear();
  preview_->clear();
  metadata_->clear();
  count_label_->setText(QString("%1 result(s)").arg(static_cast<int>(posts_.size())));
  use_reference_button_->setEnabled(!posts_.empty());

  for (std::size_t i = 0; i < posts_.size(); ++i) {
    const auto& post = posts_[i];
    auto* item = new QListWidgetItem(placeholder_icon(kThumbSize), post_title(post));
    item->setData(Qt::UserRole, static_cast<int>(i));
    item->setToolTip(metadataText(post));
    list_->addItem(item);
  }
  if (!posts_.empty()) {
    list_->setCurrentRow(0);
  }
  loadThumbnails();
}

void ResultsPage::setStatusCallback(std::function<void(QString)> callback) {
  status_callback_ = std::move(callback);
}

void ResultsPage::setReferenceCallback(std::function<void(Post)> callback) {
  reference_callback_ = std::move(callback);
}

void ResultsPage::loadThumbnails() {
  const int generation = generation_;
  for (std::size_t row = 0; row < posts_.size(); ++row) {
    const auto post = posts_[row];
    auto* watcher = new QFutureWatcher<PathWorkerResult>(this);
    connect(watcher, &QFutureWatcher<PathWorkerResult>::finished, this,
            [this, watcher, row = static_cast<int>(row), generation] {
              const auto result = watcher->result();
              watcher->deleteLater();
              if (generation != generation_ || row >= list_->count() ||
                  !result.error.empty() || result.path.empty()) {
                return;
              }
              auto pixmap = load_pixmap(result.path, kThumbSize);
              if (!pixmap.isNull()) {
                list_->item(row)->setIcon(QIcon(pixmap));
              }
            });
    watcher->setFuture(QtConcurrent::run([this, post] {
      PathWorkerResult result;
      try {
        result.path = app_.ensure_preview(post);
      } catch (const std::exception& error) {
        result.error = error.what();
      }
      return result;
    }));
  }
}

void ResultsPage::showSelectedPost() {
  const auto row = list_->currentRow();
  if (row < 0 || row >= static_cast<int>(posts_.size())) {
    preview_->clear();
    metadata_->clear();
    return;
  }

  const auto post = posts_[row];
  metadata_->setPlainText(metadataText(post));
  preview_->setMessage("Loading preview...");

  const int generation = ++preview_generation_;
  auto* watcher = new QFutureWatcher<PathWorkerResult>(this);
  connect(watcher, &QFutureWatcher<PathWorkerResult>::finished, this,
          [this, watcher, generation] {
            const auto result = watcher->result();
            watcher->deleteLater();
            if (generation != preview_generation_) {
              return;
            }
            if (!result.error.empty()) {
              preview_->setMessage(qs(result.error));
              return;
            }
            if (result.path.empty()) {
              preview_->setMessage("This post has no preview image.");
              return;
            }
            preview_->setImagePath(result.path);
          });
  watcher->setFuture(QtConcurrent::run([this, post] {
    PathWorkerResult result;
    try {
      result.path = app_.ensure_preview(post);
    } catch (const std::exception& error) {
      result.error = error.what();
    }
    return result;
  }));
}

void ResultsPage::downloadSelected() {
  const auto selected = list_->selectedItems();
  if (selected.empty()) {
    if (status_callback_) {
      status_callback_("No result selected");
    }
    return;
  }

  int queued = 0;
  for (const auto* item : selected) {
    const int index = item->data(Qt::UserRole).toInt();
    if (index < 0 || index >= static_cast<int>(posts_.size())) {
      continue;
    }
    try {
      app_.enqueue_download(posts_[index]);
      ++queued;
    } catch (const std::exception& error) {
      if (status_callback_) {
        status_callback_(qs(error.what()));
      }
    }
  }
  if (queued > 0) {
    app_.start_downloads();
  }
  if (status_callback_) {
    status_callback_(QString("Queued %1 download(s)").arg(queued));
  }
}

void ResultsPage::downloadAllVisible() {
  int queued = 0;
  for (const auto& post : posts_) {
    try {
      app_.enqueue_download(post);
      ++queued;
    } catch (const std::exception&) {
    }
  }
  if (queued > 0) {
    app_.start_downloads();
  }
  if (status_callback_) {
    status_callback_(QString("Queued %1 visible result(s)").arg(queued));
  }
}

void ResultsPage::openSelectedPostUrl() {
  const auto row = list_->currentRow();
  if (row < 0 || row >= static_cast<int>(posts_.size())) {
    return;
  }
  QDesktopServices::openUrl(QUrl(qs(posts_[row].post_url)));
}

void ResultsPage::useSelectedAsReference() {
  const auto row = list_->currentRow();
  if (row < 0 || row >= static_cast<int>(posts_.size())) {
    if (status_callback_) {
      status_callback_("No result selected");
    }
    return;
  }
  if (reference_callback_) {
    reference_callback_(posts_[row]);
  }
}

QString ResultsPage::metadataText(const Post& post) const {
  QString text;
  text += "Provider: " + qs(post.provider) + "\n";
  text += "Post ID: " + qs(post.id) + "\n";
  text += "Rating: " + rating_label(post.rating) + "\n";
  text += "Size: " + dimensions(post) + "\n";
  text += "File: " + qs(post.file_ext) + " " + bytes_label(post.file_size) + "\n";
  text += "Score: " + QString::number(post.score) + "\n";
  text += "Favorites: " + QString::number(post.favorites) + "\n";
  text += "Artists: " + qs(join(post.artist_tags, " ")) + "\n";
  text += "Tags: " + qs(join(post.tags, " ")) + "\n";
  text += "Post URL: " + qs(post.post_url) + "\n";
  text += "File URL: " + qs(post.file_url) + "\n";
  if (!post.source.empty()) {
    text += "Source: " + qs(post.source) + "\n";
  }
  return text;
}

}  // namespace boorubox::gui
