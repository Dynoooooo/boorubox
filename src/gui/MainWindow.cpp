#include "gui/MainWindow.hpp"

#include <QApplication>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QStatusBar>

#include "gui/GalleryPage.hpp"
#include "gui/LogsPage.hpp"
#include "gui/QueuePage.hpp"
#include "gui/ResultsPage.hpp"
#include "gui/SearchPage.hpp"
#include "gui/SettingsPage.hpp"

namespace boorubox::gui {

enum PageIndex {
  Search = 0,
  Results,
  Queue,
  Gallery,
  Settings,
  Logs,
};

MainWindow::MainWindow(App& app, std::filesystem::path config_path, QWidget* parent)
    : QMainWindow(parent), app_(app) {
  setWindowTitle("BooruBox");
  resize(1280, 820);
  setMinimumSize(960, 640);
  statusBar()->setSizeGripEnabled(false);

  sidebar_ = new QListWidget(this);
  sidebar_->addItems({"Search", "Results", "Queue", "Gallery", "Settings", "Logs"});
  sidebar_->setFixedWidth(150);
  sidebar_->setCurrentRow(Search);
  sidebar_->setSpacing(3);

  stack_ = new QStackedWidget(this);
  search_page_ = new SearchPage(app_, this);
  results_page_ = new ResultsPage(app_, this);
  queue_page_ = new QueuePage(app_, this);
  gallery_page_ = new GalleryPage(app_, this);
  settings_page_ = new SettingsPage(app_, std::move(config_path), this);
  logs_page_ = new LogsPage(app_, this);

  stack_->addWidget(search_page_);
  stack_->addWidget(results_page_);
  stack_->addWidget(queue_page_);
  stack_->addWidget(gallery_page_);
  stack_->addWidget(settings_page_);
  stack_->addWidget(logs_page_);

  auto* central = new QWidget(this);
  auto* layout = new QHBoxLayout(central);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(sidebar_);
  layout->addWidget(stack_, 1);
  setCentralWidget(central);

  const auto status = [this](const QString& message) { showStatus(message); };
  search_page_->setStatusCallback(status);
  results_page_->setStatusCallback(status);
  queue_page_->setStatusCallback(status);
  gallery_page_->setStatusCallback(status);
  settings_page_->setStatusCallback([this](const QString& message) {
    showStatus(message);
    search_page_->refreshProviders();
  });

  search_page_->setResultsCallback([this](std::vector<Post> posts) {
    results_page_->setPosts(std::move(posts));
    sidebar_->setCurrentRow(Results);
    showPage(Results);
  });
  results_page_->setReferenceCallback([this](const Post& post) {
    search_page_->setReference(post);
    sidebar_->setCurrentRow(Search);
    showPage(Search);
    showStatus("Reference image loaded from search result");
  });
  gallery_page_->setReferenceCallback([this](const LocalArchiveItem& item) {
    search_page_->setReference(item);
    sidebar_->setCurrentRow(Search);
    showPage(Search);
    showStatus("Reference image loaded from local gallery");
  });
  search_page_->setSafetyChangedCallback([this](bool sfw_mode_enabled) {
    settings_page_->syncNsfwEnabled(!sfw_mode_enabled);
    statusBar()->showMessage(sfw_mode_enabled ? "SFW mode active"
                                              : "SFW mode off",
                             7000);
  });

  connect(sidebar_, &QListWidget::currentRowChanged, this,
          [this](int index) { showPage(index); });

  statusBar()->showMessage(app_.config().enable_nsfw
                               ? "NSFW mode enabled"
                               : "SFW mode active");
  applyStyle();
}

void MainWindow::showPage(int index) {
  if (index < 0 || index >= stack_->count()) {
    return;
  }
  stack_->setCurrentIndex(index);
  if (index == Gallery) {
    gallery_page_->refresh();
  }
  if (index == Queue) {
    queue_page_->refresh();
  }
  if (index == Logs) {
    logs_page_->refresh();
  }
}

void MainWindow::showStatus(const QString& message) {
  statusBar()->showMessage(message, 7000);
}

void MainWindow::applyStyle() {
  qApp->setStyleSheet(R"(
    QMainWindow {
      background: palette(window);
    }
    QListWidget {
      border: none;
      background: palette(alternate-base);
      padding: 10px;
      font-size: 14px;
    }
    QListWidget::item {
      padding: 10px 12px;
      border-radius: 6px;
    }
    QListWidget::item:selected {
      background: palette(highlight);
      color: palette(highlighted-text);
    }
    QGroupBox {
      border: 1px solid palette(mid);
      border-radius: 8px;
      margin-top: 12px;
      padding: 10px;
      font-weight: 600;
    }
    QGroupBox::title {
      subcontrol-origin: margin;
      left: 10px;
      padding: 0 4px;
    }
    QPushButton {
      padding: 7px 12px;
      border-radius: 5px;
    }
    QLineEdit, QComboBox, QSpinBox, QTextEdit, QTableWidget {
      border: 1px solid palette(mid);
      border-radius: 5px;
      padding: 5px;
      background: palette(base);
    }
  )");
}

}  // namespace boorubox::gui
