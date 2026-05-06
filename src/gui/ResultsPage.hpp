#pragma once

#include <functional>
#include <vector>

#include <QFutureWatcher>
#include <QWidget>

#include "app/App.hpp"
#include "gui/ImagePreviewWidget.hpp"

class QLabel;
class QListWidget;
class QPushButton;
class QTextEdit;

namespace boorubox::gui {

struct PathWorkerResult {
  std::filesystem::path path;
  std::string error;
};

class ResultsPage final : public QWidget {
 public:
  explicit ResultsPage(App& app, QWidget* parent = nullptr);

  void setPosts(std::vector<Post> posts);
  void setStatusCallback(std::function<void(QString)> callback);
  void setReferenceCallback(std::function<void(Post)> callback);

 private:
  void loadThumbnails();
  void showSelectedPost();
  void downloadSelected();
  void downloadAllVisible();
  void openSelectedPostUrl();
  void useSelectedAsReference();
  QString metadataText(const Post& post) const;

  App& app_;
  QListWidget* list_ = nullptr;
  ImagePreviewWidget* preview_ = nullptr;
  QTextEdit* metadata_ = nullptr;
  QPushButton* download_selected_button_ = nullptr;
  QPushButton* download_all_button_ = nullptr;
  QPushButton* open_url_button_ = nullptr;
  QPushButton* use_reference_button_ = nullptr;
  QLabel* count_label_ = nullptr;
  std::vector<Post> posts_;
  int generation_ = 0;
  int preview_generation_ = 0;
  std::function<void(QString)> status_callback_;
  std::function<void(Post)> reference_callback_;
};

}  // namespace boorubox::gui
