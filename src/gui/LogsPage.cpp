#include "gui/LogsPage.hpp"

#include <QApplication>
#include <QClipboard>
#include <QPushButton>
#include <QTextEdit>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "gui/QtUtil.hpp"

namespace boorubox::gui {

LogsPage::LogsPage(App& app, QWidget* parent) : QWidget(parent), app_(app) {
  log_edit_ = new QTextEdit(this);
  log_edit_->setReadOnly(true);
  log_edit_->setLineWrapMode(QTextEdit::NoWrap);
  refresh_button_ = new QPushButton("Refresh", this);
  copy_button_ = new QPushButton("Copy All", this);

  auto* toolbar = new QHBoxLayout;
  toolbar->addStretch();
  toolbar->addWidget(copy_button_);
  toolbar->addWidget(refresh_button_);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 14, 14, 14);
  layout->addLayout(toolbar);
  layout->addWidget(log_edit_, 1);

  timer_ = new QTimer(this);
  timer_->setInterval(1500);
  connect(timer_, &QTimer::timeout, this, [this] { refresh(); });
  connect(refresh_button_, &QPushButton::clicked, this, [this] { refresh(); });
  connect(copy_button_, &QPushButton::clicked, this, [this] { copyAll(); });
  timer_->start();
  refresh();
}

void LogsPage::refresh() {
  QString text;
  for (const auto& line : app_.logs()) {
    text += qs(line) + "\n";
  }
  if (text == last_text_) {
    return;
  }
  if (log_edit_->hasFocus() && log_edit_->textCursor().hasSelection()) {
    return;
  }
  last_text_ = text;
  const bool was_at_end = log_edit_->textCursor().atEnd();
  log_edit_->setPlainText(text);
  if (was_at_end) {
    log_edit_->moveCursor(QTextCursor::End);
  }
}

void LogsPage::copyAll() {
  QApplication::clipboard()->setText(log_edit_->toPlainText());
}

}  // namespace boorubox::gui
