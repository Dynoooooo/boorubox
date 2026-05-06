#pragma once

#include <QWidget>

#include "app/App.hpp"

class QPushButton;
class QTextEdit;
class QTimer;

namespace boorubox::gui {

class LogsPage final : public QWidget {
 public:
  explicit LogsPage(App& app, QWidget* parent = nullptr);
  void refresh();

 private:
  void copyAll();

  App& app_;
  QTextEdit* log_edit_ = nullptr;
  QPushButton* refresh_button_ = nullptr;
  QPushButton* copy_button_ = nullptr;
  QTimer* timer_ = nullptr;
  QString last_text_;
};

}  // namespace boorubox::gui
