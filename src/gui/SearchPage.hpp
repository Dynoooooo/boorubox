#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QFutureWatcher>
#include <QWidget>

#include "app/App.hpp"

class QComboBox;
class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace boorubox::gui {

struct SearchWorkerResult {
  std::vector<Post> posts;
  std::string error;
  std::string warning;
};

class SearchPage final : public QWidget {
 public:
  explicit SearchPage(App& app, QWidget* parent = nullptr);

  void refreshProviders();
  void setReference(const Post& post);
  void setReference(const LocalArchiveItem& item);
  void setResultsCallback(std::function<void(std::vector<Post>)> callback);
  void setStatusCallback(std::function<void(QString)> callback);
  void setSafetyChangedCallback(std::function<void(bool)> callback);

 private:
  void setReferenceDetails(const QString& title,
                           const std::vector<std::string>& tags,
                           const std::string& provider_name);
  void applyReferenceTags();
  void clearReference();
  void startSearch();
  void clearInputs();
  void setSfwMode(bool enabled);
  void updateRatingChoices();
  void updateSafetyLabel();

  App& app_;
  QCheckBox* sfw_mode_check_ = nullptr;
  QComboBox* provider_box_ = nullptr;
  QLineEdit* tags_edit_ = nullptr;
  QLineEdit* excluded_edit_ = nullptr;
  QComboBox* rating_box_ = nullptr;
  QSpinBox* page_spin_ = nullptr;
  QSpinBox* limit_spin_ = nullptr;
  QLabel* safety_label_ = nullptr;
  QGroupBox* reference_box_ = nullptr;
  QLabel* reference_label_ = nullptr;
  QPushButton* use_reference_tags_button_ = nullptr;
  QPushButton* clear_reference_button_ = nullptr;
  QPushButton* search_button_ = nullptr;
  QPushButton* clear_button_ = nullptr;
  QFutureWatcher<SearchWorkerResult>* search_watcher_ = nullptr;
  std::vector<std::string> reference_tags_;
  std::string reference_provider_;
  std::function<void(std::vector<Post>)> results_callback_;
  std::function<void(QString)> status_callback_;
  std::function<void(bool)> safety_changed_callback_;
};

}  // namespace boorubox::gui
