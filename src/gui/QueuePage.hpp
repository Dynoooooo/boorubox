#pragma once

#include <functional>
#include <optional>

#include <QWidget>

#include "app/App.hpp"

class QPushButton;
class QTableWidget;
class QTimer;

namespace boorubox::gui {

class QueuePage final : public QWidget {
 public:
  explicit QueuePage(App& app, QWidget* parent = nullptr);

  void refresh();
  void setStatusCallback(std::function<void(QString)> callback);

 private:
  std::optional<std::size_t> selectedJobId() const;
  void retrySelected();
  void cancelSelected();
  void clearFailedAndSkipped();

  App& app_;
  QTableWidget* table_ = nullptr;
  QPushButton* retry_button_ = nullptr;
  QPushButton* cancel_button_ = nullptr;
  QPushButton* clear_failed_skipped_button_ = nullptr;
  QTimer* timer_ = nullptr;
  std::function<void(QString)> status_callback_;
};

}  // namespace boorubox::gui
