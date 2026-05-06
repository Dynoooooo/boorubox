#pragma once

#include <filesystem>

#include <QMainWindow>

#include "app/App.hpp"

class QListWidget;
class QStackedWidget;

namespace boorubox::gui {

class GalleryPage;
class LogsPage;
class QueuePage;
class ResultsPage;
class SearchPage;
class SettingsPage;

class MainWindow final : public QMainWindow {
 public:
  MainWindow(App& app, std::filesystem::path config_path,
             QWidget* parent = nullptr);

 private:
  void showPage(int index);
  void showStatus(const QString& message);
  void applyStyle();

  App& app_;
  QListWidget* sidebar_ = nullptr;
  QStackedWidget* stack_ = nullptr;
  SearchPage* search_page_ = nullptr;
  ResultsPage* results_page_ = nullptr;
  QueuePage* queue_page_ = nullptr;
  GalleryPage* gallery_page_ = nullptr;
  SettingsPage* settings_page_ = nullptr;
  LogsPage* logs_page_ = nullptr;
};

}  // namespace boorubox::gui
