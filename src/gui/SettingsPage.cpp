#include "gui/SettingsPage.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "app/Config.hpp"
#include "gui/QtUtil.hpp"
#include "gui/SafetyUtil.hpp"
#include "util/StringUtil.hpp"

namespace boorubox::gui {

SettingsPage::SettingsPage(App& app, std::filesystem::path config_path,
                           QWidget* parent)
    : QWidget(parent),
      app_(app),
      config_path_(std::move(config_path)),
      shown_config_(app.config()) {
  download_dir_edit_ = new QLineEdit(this);
  cache_dir_edit_ = new QLineEdit(this);
  index_path_edit_ = new QLineEdit(this);
  user_agent_edit_ = new QLineEdit(this);

  max_downloads_spin_ = new QSpinBox(this);
  max_downloads_spin_->setRange(1, 16);

  rating_box_ = new QComboBox(this);

  quality_box_ = new QComboBox(this);
  quality_box_->addItems({"original", "sample", "preview"});

  nsfw_check_ = new QCheckBox("Enable NSFW providers and searches", this);

  blacklist_edit_ = new QTextEdit(this);
  blacklist_edit_->setMinimumHeight(110);
  blacklist_edit_->setPlaceholderText("One tag per line");

  auto* app_form = new QFormLayout;
  app_form->addRow("Download directory", download_dir_edit_);
  app_form->addRow("Cache directory", cache_dir_edit_);
  app_form->addRow("Index path", index_path_edit_);
  app_form->addRow("User-Agent", user_agent_edit_);
  app_form->addRow("Max concurrent downloads", max_downloads_spin_);
  app_form->addRow("Default rating", rating_box_);
  app_form->addRow("Download quality", quality_box_);
  app_form->addRow("", nsfw_check_);
  app_form->addRow("Blacklisted tags", blacklist_edit_);

  auto* app_group = new QGroupBox("Application", this);
  app_group->setLayout(app_form);

  providers_table_ = new QTableWidget(this);
  providers_table_->setColumnCount(6);
  providers_table_->setHorizontalHeaderLabels(
      {"Enabled", "Provider", "Base URL", "NSFW", "Login", "API Key"});
  providers_table_->horizontalHeader()->setStretchLastSection(true);
  providers_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  providers_table_->setMinimumHeight(180);

  auto* providers_group = new QGroupBox("Providers", this);
  auto* providers_layout = new QVBoxLayout(providers_group);
  providers_layout->addWidget(providers_table_);

  save_button_ = new QPushButton("Save", this);
  reload_button_ = new QPushButton("Reload From Disk", this);
  auto* buttons = new QHBoxLayout;
  buttons->addStretch();
  buttons->addWidget(reload_button_);
  buttons->addWidget(save_button_);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 18, 18, 18);
  layout->setSpacing(14);
  layout->addWidget(app_group);
  layout->addWidget(providers_group);
  layout->addLayout(buttons);
  layout->addStretch();

  connect(save_button_, &QPushButton::clicked, this, [this] { saveConfig(); });
  connect(reload_button_, &QPushButton::clicked, this, [this] { reloadConfig(); });
  connect(nsfw_check_, &QCheckBox::toggled, this,
          [this](bool enabled) { handleNsfwToggled(enabled); });

  loadFromConfig(shown_config_);
}

void SettingsPage::setStatusCallback(std::function<void(QString)> callback) {
  status_callback_ = std::move(callback);
}

void SettingsPage::syncNsfwEnabled(bool enabled) {
  shown_config_.enable_nsfw = enabled;
  const QSignalBlocker blocker(nsfw_check_);
  nsfw_check_->setChecked(enabled);
  refreshRatingChoices();
}

void SettingsPage::loadFromConfig(const AppConfig& config) {
  shown_config_ = config;
  download_dir_edit_->setText(qs(config.download_dir));
  cache_dir_edit_->setText(qs(config.cache_dir));
  index_path_edit_->setText(qs(config.index_path));
  user_agent_edit_->setText(qs(config.user_agent));
  max_downloads_spin_->setValue(config.max_concurrent_downloads);
  quality_box_->setCurrentText(qs(config.preferred_download_quality));
  {
    const QSignalBlocker blocker(nsfw_check_);
    nsfw_check_->setChecked(config.enable_nsfw);
  }
  refreshRatingChoices();
  rating_box_->setCurrentText(rating_label(config.default_rating));
  if (rating_box_->currentText().isEmpty() ||
      rating_box_->findText(rating_label(config.default_rating)) < 0) {
    rating_box_->setCurrentText("safe");
  }
  blacklist_edit_->setPlainText(qs(join(config.blacklisted_tags, "\n")));

  providers_table_->setRowCount(static_cast<int>(config.providers.size()));
  int row = 0;
  for (const auto& [name, provider] : config.providers) {
    auto* enabled = new QTableWidgetItem;
    enabled->setCheckState(provider.enabled ? Qt::Checked : Qt::Unchecked);
    providers_table_->setItem(row, 0, enabled);
    providers_table_->setItem(row, 1, new QTableWidgetItem(qs(name)));
    providers_table_->setItem(row, 2, new QTableWidgetItem(qs(provider.base_url)));

    auto* nsfw = new QTableWidgetItem;
    nsfw->setCheckState(provider.nsfw_provider ? Qt::Checked : Qt::Unchecked);
    providers_table_->setItem(row, 3, nsfw);
    providers_table_->setItem(row, 4, new QTableWidgetItem(qs(provider.login)));
    providers_table_->setItem(row, 5, new QTableWidgetItem(qs(provider.api_key)));
    ++row;
  }
}

AppConfig SettingsPage::readConfigFromWidgets() const {
  AppConfig config = shown_config_;
  config.download_dir = to_std_string(download_dir_edit_->text());
  config.cache_dir = to_std_string(cache_dir_edit_->text());
  config.index_path = to_std_string(index_path_edit_->text());
  config.user_agent = to_std_string(user_agent_edit_->text());
  config.max_concurrent_downloads = max_downloads_spin_->value();
  config.default_rating = rating_from_label(rating_box_->currentText());
  config.preferred_download_quality = to_std_string(quality_box_->currentText());
  config.enable_nsfw = nsfw_check_->isChecked();
  config.blacklisted_tags.clear();
  for (const auto& line : split(to_std_string(blacklist_edit_->toPlainText()), '\n')) {
    const auto tag = trim(line);
    if (!tag.empty()) {
      config.blacklisted_tags.push_back(tag);
    }
  }

  config.providers.clear();
  for (int row = 0; row < providers_table_->rowCount(); ++row) {
    const auto name_item = providers_table_->item(row, 1);
    if (name_item == nullptr || name_item->text().trimmed().isEmpty()) {
      continue;
    }
    ProviderConfig provider;
    provider.enabled = providers_table_->item(row, 0) != nullptr &&
                       providers_table_->item(row, 0)->checkState() == Qt::Checked;
    provider.base_url = providers_table_->item(row, 2) == nullptr
                            ? std::string{}
                            : to_std_string(providers_table_->item(row, 2)->text());
    provider.nsfw_provider = providers_table_->item(row, 3) != nullptr &&
                             providers_table_->item(row, 3)->checkState() == Qt::Checked;
    provider.login = providers_table_->item(row, 4) == nullptr
                         ? std::string{}
                         : to_std_string(providers_table_->item(row, 4)->text());
    provider.api_key = providers_table_->item(row, 5) == nullptr
                           ? std::string{}
                           : to_std_string(providers_table_->item(row, 5)->text());
    config.providers[to_std_string(name_item->text())] = std::move(provider);
  }
  return config;
}

void SettingsPage::handleNsfwToggled(bool enabled) {
  if (enabled && !confirm_nsfw_mode(this, app_.config())) {
    const QSignalBlocker blocker(nsfw_check_);
    nsfw_check_->setChecked(false);
    return;
  }
  shown_config_.enable_nsfw = enabled;
  app_.set_enable_nsfw(enabled);
  refreshRatingChoices();
  if (status_callback_) {
    status_callback_(enabled
                         ? "SFW mode is off. Save settings to keep this after restart."
                         : "SFW mode is on. Sensitive ratings are hidden.");
  }
}

void SettingsPage::refreshRatingChoices() {
  const bool sfw_mode = !nsfw_check_->isChecked();
  const auto current = rating_box_->currentText();
  const QSignalBlocker blocker(rating_box_);
  populate_rating_choices(*rating_box_, sfw_mode, /*include_unknown=*/true);
  if (!current.isEmpty() && rating_box_->findText(current) >= 0) {
    rating_box_->setCurrentText(current);
  } else {
    rating_box_->setCurrentText("safe");
  }
}

void SettingsPage::saveConfig() {
  try {
    const auto config = readConfigFromWidgets();
    Config::save(config, config_path_);
    shown_config_ = config;
    app_.apply_runtime_config(config);
    if (status_callback_) {
      status_callback_("Settings saved and applied.");
    }
  } catch (const std::exception& error) {
    QMessageBox::critical(this, "Save Failed", qs(error.what()));
  }
}

void SettingsPage::reloadConfig() {
  try {
    const auto config = Config::load(config_path_);
    loadFromConfig(config);
    app_.apply_runtime_config(config);
    if (status_callback_) {
      status_callback_("Settings reloaded from disk.");
    }
  } catch (const std::exception& error) {
    QMessageBox::critical(this, "Reload Failed", qs(error.what()));
  }
}

}  // namespace boorubox::gui
