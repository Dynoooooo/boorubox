#pragma once

#include <functional>
#include <vector>

#include <QFutureWatcher>
#include <QWidget>

#include "app/App.hpp"
#include "gui/ImagePreviewWidget.hpp"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTextEdit;

namespace boorubox::gui {

struct GalleryWorkerResult {
  std::vector<LocalArchiveItem> items;
  std::string error;
};

class GalleryPage final : public QWidget {
 public:
  explicit GalleryPage(App& app, QWidget* parent = nullptr);

  void refresh();
  void setStatusCallback(std::function<void(QString)> callback);
  void setReferenceCallback(std::function<void(LocalArchiveItem)> callback);

 private:
  void applyFilters();
  void rebuildIndex();
  void showSelectedItem();
  void openSelectedFile();
  void useSelectedAsReference();
  QString metadataText(const LocalArchiveItem& item) const;

  App& app_;
  QLineEdit* tag_filter_ = nullptr;
  QComboBox* provider_filter_ = nullptr;
  QComboBox* rating_filter_ = nullptr;
  QCheckBox* favorites_filter_ = nullptr;
  QComboBox* sort_box_ = nullptr;
  QPushButton* refresh_button_ = nullptr;
  QPushButton* rebuild_button_ = nullptr;
  QPushButton* open_file_button_ = nullptr;
  QPushButton* use_reference_button_ = nullptr;
  QLabel* count_label_ = nullptr;
  QListWidget* list_ = nullptr;
  ImagePreviewWidget* preview_ = nullptr;
  QTextEdit* metadata_ = nullptr;
  QFutureWatcher<GalleryWorkerResult>* refresh_watcher_ = nullptr;
  std::vector<LocalArchiveItem> all_items_;
  std::vector<LocalArchiveItem> visible_items_;
  std::function<void(QString)> status_callback_;
  std::function<void(LocalArchiveItem)> reference_callback_;
};

}  // namespace boorubox::gui
