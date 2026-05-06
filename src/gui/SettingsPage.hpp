#pragma once

#include <filesystem>
#include <functional>

#include <QWidget>

#include "app/App.hpp"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTextEdit;

namespace boorubox::gui {

class SettingsPage final : public QWidget {
 public:
  SettingsPage(App& app, std::filesystem::path config_path,
               QWidget* parent = nullptr);

  void setStatusCallback(std::function<void(QString)> callback);
  void syncNsfwEnabled(bool enabled);

 private:
  void loadFromConfig(const AppConfig& config);
  AppConfig readConfigFromWidgets() const;
  void handleNsfwToggled(bool enabled);
  void refreshRatingChoices();
  void saveConfig();
  void reloadConfig();

  App& app_;
  std::filesystem::path config_path_;
  AppConfig shown_config_;
  QLineEdit* download_dir_edit_ = nullptr;
  QLineEdit* cache_dir_edit_ = nullptr;
  QLineEdit* index_path_edit_ = nullptr;
  QLineEdit* user_agent_edit_ = nullptr;
  QSpinBox* max_downloads_spin_ = nullptr;
  QComboBox* rating_box_ = nullptr;
  QComboBox* quality_box_ = nullptr;
  QCheckBox* nsfw_check_ = nullptr;
  QTextEdit* blacklist_edit_ = nullptr;
  QTableWidget* providers_table_ = nullptr;
  QPushButton* save_button_ = nullptr;
  QPushButton* reload_button_ = nullptr;
  std::function<void(QString)> status_callback_;
};

}  // namespace boorubox::gui
