#include "gui/SearchPage.hpp"

#include <algorithm>
#include <iterator>
#include <unordered_set>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFuture>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include "gui/QtUtil.hpp"
#include "gui/SafetyUtil.hpp"
#include "util/StringUtil.hpp"

namespace boorubox::gui {

namespace {

constexpr const char* kAllProvidersValue = "__all_enabled_providers__";
constexpr std::size_t kMaxReferenceTags = 16;

void add_rating_items(QComboBox& combo, bool sfw_mode) {
  combo.clear();
  combo.addItem("safe");
  combo.addItem("general");
  if (!sfw_mode) {
    combo.addItem("sensitive");
    combo.addItem("questionable");
    combo.addItem("explicit");
  }
}

bool rating_needs_nsfw_provider(Rating rating) {
  return rating == Rating::Sensitive || rating == Rating::Questionable ||
         rating == Rating::Explicit;
}

bool is_sfw_only_provider(std::string_view provider_name) {
  return provider_name == "safebooru" || provider_name == "e926";
}

std::vector<std::string> filtered_reference_tags(
    const std::vector<std::string>& tags,
    const std::vector<std::string>& blacklisted_tags) {
  std::unordered_set<std::string> seen;
  std::unordered_set<std::string> blacklist;
  for (const auto& tag : blacklisted_tags) {
    blacklist.insert(lower(tag));
  }

  std::vector<std::string> out;
  for (const auto& raw_tag : tags) {
    auto tag = trim(raw_tag);
    if (tag.empty() || tag.starts_with("-")) {
      continue;
    }
    const auto normalized = lower(tag);
    if (tag.find(':') != std::string::npos || blacklist.contains(normalized) ||
        !seen.insert(normalized).second) {
      continue;
    }
    out.push_back(std::move(tag));
    if (out.size() >= kMaxReferenceTags) {
      break;
    }
  }
  return out;
}

std::vector<std::string> combined_tags(const std::vector<std::string>& first,
                                       const std::vector<std::string>& second) {
  auto tags = first;
  tags.insert(tags.end(), second.begin(), second.end());
  return tags;
}

}  // namespace

SearchPage::SearchPage(App& app, QWidget* parent) : QWidget(parent), app_(app) {
  sfw_mode_check_ = new QCheckBox("SFW mode", this);
  sfw_mode_check_->setChecked(!app_.config().enable_nsfw);

  provider_box_ = new QComboBox(this);
  tags_edit_ = new QLineEdit(this);
  tags_edit_->setPlaceholderText("cat landscape score:>10");
  excluded_edit_ = new QLineEdit(this);
  excluded_edit_->setPlaceholderText("watermark gore");

  rating_box_ = new QComboBox(this);
  updateRatingChoices();

  page_spin_ = new QSpinBox(this);
  page_spin_->setRange(1, 100000);
  page_spin_->setValue(1);

  limit_spin_ = new QSpinBox(this);
  limit_spin_->setRange(1, 200);
  limit_spin_->setValue(20);

  safety_label_ = new QLabel(this);
  safety_label_->setWordWrap(true);

  search_button_ = new QPushButton("Search", this);
  search_button_->setDefault(true);
  search_button_->setAutoDefault(true);
  clear_button_ = new QPushButton("Clear", this);

  auto* form = new QFormLayout;
  form->setLabelAlignment(Qt::AlignRight);
  form->addRow("", sfw_mode_check_);
  form->addRow("Provider", provider_box_);
  form->addRow("Tags", tags_edit_);
  form->addRow("Exclude", excluded_edit_);
  form->addRow("Rating", rating_box_);
  form->addRow("Page", page_spin_);
  form->addRow("Limit", limit_spin_);

  auto* buttons = new QHBoxLayout;
  buttons->addStretch();
  buttons->addWidget(clear_button_);
  buttons->addWidget(search_button_);

  auto* search_box = new QGroupBox("Search", this);
  auto* search_layout = new QVBoxLayout(search_box);
  search_layout->addLayout(form);
  search_layout->addLayout(buttons);

  auto* safety_box = new QGroupBox("Safety", this);
  auto* safety_layout = new QVBoxLayout(safety_box);
  safety_layout->addWidget(safety_label_);

  reference_box_ = new QGroupBox("Reference Image", this);
  reference_label_ = new QLabel("No reference image selected.", this);
  reference_label_->setWordWrap(true);
  use_reference_tags_button_ = new QPushButton("Use Reference Tags", this);
  clear_reference_button_ = new QPushButton("Clear Reference", this);
  use_reference_tags_button_->setEnabled(false);
  clear_reference_button_->setEnabled(false);

  auto* reference_buttons = new QHBoxLayout;
  reference_buttons->addStretch();
  reference_buttons->addWidget(clear_reference_button_);
  reference_buttons->addWidget(use_reference_tags_button_);

  auto* reference_layout = new QVBoxLayout(reference_box_);
  reference_layout->addWidget(reference_label_);
  reference_layout->addLayout(reference_buttons);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 18, 18, 18);
  layout->setSpacing(14);
  layout->addWidget(search_box);
  layout->addWidget(reference_box_);
  layout->addWidget(safety_box);
  layout->addStretch();

  search_watcher_ = new QFutureWatcher<SearchWorkerResult>(this);
  connect(search_button_, &QPushButton::clicked, this,
          [this] { startSearch(); });
  connect(tags_edit_, &QLineEdit::returnPressed, this,
          [this] { startSearch(); });
  connect(excluded_edit_, &QLineEdit::returnPressed, this,
          [this] { startSearch(); });
  connect(clear_button_, &QPushButton::clicked, this, [this] { clearInputs(); });
  connect(use_reference_tags_button_, &QPushButton::clicked, this,
          [this] { applyReferenceTags(); });
  connect(clear_reference_button_, &QPushButton::clicked, this,
          [this] { clearReference(); });
  connect(sfw_mode_check_, &QCheckBox::toggled, this,
          [this](bool checked) { setSfwMode(checked); });
  connect(search_watcher_, &QFutureWatcher<SearchWorkerResult>::finished, this,
          [this] {
            search_button_->setEnabled(true);
            const auto result = search_watcher_->result();
            if (!result.error.empty()) {
              if (status_callback_) {
                status_callback_(qs(result.error));
              }
              return;
            }
            if (status_callback_) {
              auto message = QString("Found %1 result(s)")
                                 .arg(static_cast<int>(result.posts.size()));
              if (!result.warning.empty()) {
                message += ". " + qs(result.warning);
              }
              status_callback_(message);
            }
            if (results_callback_) {
              results_callback_(result.posts);
            }
          });

  auto* return_shortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
  return_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(return_shortcut, &QShortcut::activated, this,
          [this] { startSearch(); });

  auto* enter_shortcut = new QShortcut(QKeySequence(Qt::Key_Enter), this);
  enter_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(enter_shortcut, &QShortcut::activated, this,
          [this] { startSearch(); });

  refreshProviders();
  updateSafetyLabel();
}

void SearchPage::setReference(const Post& post) {
  setReferenceDetails(
      QString("%1 / %2 (%3)")
          .arg(qs(post.provider), qs(post.id), rating_label(post.rating)),
      combined_tags(post.artist_tags, post.tags), post.provider);
}

void SearchPage::setReference(const LocalArchiveItem& item) {
  const auto id = item.post_id.empty()
                      ? item.local_file_path.filename().string()
                      : item.post_id;
  setReferenceDetails(
      QString("%1 / %2 (%3)")
          .arg(qs(item.provider), qs(id), rating_label(item.rating)),
      combined_tags(item.artists, item.tags), item.provider);
}

void SearchPage::refreshProviders() {
  const auto current = provider_box_->currentText();
  const auto current_value = provider_box_->currentData().toString();
  provider_box_->clear();
  provider_box_->addItem("All enabled providers", kAllProvidersValue);
  for (const auto& name : app_.provider_names()) {
    provider_box_->addItem(qs(name), qs(name));
  }
  if (!current_value.isEmpty()) {
    const auto index = provider_box_->findData(current_value);
    if (index >= 0) {
      provider_box_->setCurrentIndex(index);
    }
  } else if (!current.isEmpty()) {
    provider_box_->setCurrentText(current);
  }
  {
    const QSignalBlocker blocker(sfw_mode_check_);
    sfw_mode_check_->setChecked(!app_.config().enable_nsfw);
  }
  updateRatingChoices();
  updateSafetyLabel();
}

void SearchPage::setReferenceDetails(const QString& title,
                                     const std::vector<std::string>& tags,
                                     const std::string& provider_name) {
  reference_provider_ = provider_name;
  reference_tags_ =
      filtered_reference_tags(tags, app_.config().blacklisted_tags);

  if (const int index = provider_box_->findData(qs(provider_name)); index >= 0) {
    provider_box_->setCurrentIndex(index);
  }

  use_reference_tags_button_->setEnabled(!reference_tags_.empty());
  clear_reference_button_->setEnabled(true);
  if (reference_tags_.empty()) {
    reference_label_->setText(title +
                              "\nNo searchable metadata tags were available.");
    return;
  }

  reference_label_->setText(
      title + QString("\nSeed tags: %1").arg(qs(join(reference_tags_, " "))));
  if (status_callback_) {
    status_callback_(
        "Reference image selected. Click Use Reference Tags to copy its tags.");
  }
}

void SearchPage::applyReferenceTags() {
  if (reference_tags_.empty()) {
    return;
  }
  tags_edit_->setText(qs(join(reference_tags_, " ")));
  page_spin_->setValue(1);
  if (const int index = provider_box_->findData(qs(reference_provider_));
      index >= 0) {
    provider_box_->setCurrentIndex(index);
  }
  if (status_callback_) {
    status_callback_("Reference tags loaded into Search. Edit them or press Search.");
  }
}

void SearchPage::clearReference() {
  reference_tags_.clear();
  reference_provider_.clear();
  reference_label_->setText("No reference image selected.");
  use_reference_tags_button_->setEnabled(false);
  clear_reference_button_->setEnabled(false);
}

void SearchPage::setResultsCallback(std::function<void(std::vector<Post>)> callback) {
  results_callback_ = std::move(callback);
}

void SearchPage::setStatusCallback(std::function<void(QString)> callback) {
  status_callback_ = std::move(callback);
}

void SearchPage::setSafetyChangedCallback(std::function<void(bool)> callback) {
  safety_changed_callback_ = std::move(callback);
}

void SearchPage::startSearch() {
  if (provider_box_->currentText().isEmpty() || search_watcher_->isRunning()) {
    return;
  }

  SearchQuery query;
  const auto provider_value = provider_box_->currentData().toString();
  query.provider_name = provider_value.isEmpty()
                            ? to_std_string(provider_box_->currentText())
                            : to_std_string(provider_value);
  query.tags = split_words(to_std_string(tags_edit_->text()));
  query.excluded_tags = split_words(to_std_string(excluded_edit_->text()));
  query.rating_filter = rating_from_label(rating_box_->currentText());
  query.page = page_spin_->value();
  query.limit = limit_spin_->value();
  auto all_provider_names =
      query.provider_name == kAllProvidersValue ? app_.provider_names()
                                                : std::vector<std::string>{};
  if (query.provider_name == kAllProvidersValue &&
      rating_needs_nsfw_provider(query.rating_filter)) {
    std::erase_if(all_provider_names, [&](const std::string& provider_name) {
      const bool skip = is_sfw_only_provider(provider_name);
      if (skip) {
        app_.log_info("Skipping " + provider_name +
                      " for non-SFW rating search; provider is SFW-only");
      }
      return skip;
    });
    if (const auto e621 = app_.config().providers.find("e621");
        e621 == app_.config().providers.end() || !e621->second.enabled) {
      app_.log_warn(
          "e621 is disabled. Enable it in Settings and save to include it in "
          "all-provider non-SFW searches.");
    }
  }

  search_button_->setEnabled(false);
  if (status_callback_) {
    status_callback_(query.provider_name == kAllProvidersValue
                         ? "Searching all enabled providers..."
                         : "Searching...");
  }

  search_watcher_->setFuture(QtConcurrent::run([this, query, all_provider_names] {
    SearchWorkerResult result;
    if (query.provider_name == kAllProvidersValue) {
      app_.log_info("Search across all enabled providers: " +
                    std::to_string(all_provider_names.size()) + " provider(s)");
      std::vector<std::string> errors;
      int completed_providers = 0;
      for (const auto& provider_name : all_provider_names) {
        SearchQuery provider_query = query;
        provider_query.provider_name = provider_name;
        try {
          auto posts = app_.search(provider_query);
          ++completed_providers;
          result.posts.insert(result.posts.end(),
                              std::make_move_iterator(posts.begin()),
                              std::make_move_iterator(posts.end()));
        } catch (const std::exception& error) {
          const auto message = provider_name + ": " + error.what();
          errors.push_back(message);
          app_.log_warn("Search failed on " + message);
        }
      }
      app_.log_info("All-provider search completed: " +
                    std::to_string(result.posts.size()) +
                    " combined result(s), " +
                    std::to_string(completed_providers) +
                    " provider(s) completed, " + std::to_string(errors.size()) +
                    " provider(s) failed");
      if (completed_providers == 0 && !errors.empty()) {
        result.error = join(errors, "\n");
      } else if (!errors.empty()) {
        result.warning =
            QString("%1 provider(s) failed; see Logs for details.")
                .arg(static_cast<int>(errors.size()))
                .toStdString();
      }
    } else {
      try {
        result.posts = app_.search(query);
      } catch (const std::exception& error) {
        result.error = error.what();
      }
    }
    return result;
  }));
}

void SearchPage::clearInputs() {
  tags_edit_->clear();
  excluded_edit_->clear();
  page_spin_->setValue(1);
  limit_spin_->setValue(20);
}

void SearchPage::setSfwMode(bool enabled) {
  if (!enabled && !confirm_nsfw_mode(this, app_.config())) {
    const QSignalBlocker blocker(sfw_mode_check_);
    sfw_mode_check_->setChecked(true);
    return;
  }

  app_.set_enable_nsfw(!enabled);
  refreshProviders();
  if (status_callback_) {
    status_callback_(enabled
                         ? "SFW mode is on. Sensitive ratings are hidden."
                         : "SFW mode is off. Blacklists and rating filters still apply.");
  }
  if (safety_changed_callback_) {
    safety_changed_callback_(enabled);
  }
}

void SearchPage::updateRatingChoices() {
  const bool sfw_mode = !app_.config().enable_nsfw;
  const auto current = rating_box_->currentText();
  const QSignalBlocker blocker(rating_box_);
  add_rating_items(*rating_box_, sfw_mode);
  if (!current.isEmpty() && rating_box_->findText(current) >= 0) {
    rating_box_->setCurrentText(current);
  } else if (const auto fallback = rating_label(app_.config().default_rating);
             rating_box_->findText(fallback) >= 0) {
    rating_box_->setCurrentText(fallback);
  } else {
    rating_box_->setCurrentText("safe");
  }
}

void SearchPage::updateSafetyLabel() {
  if (app_.config().enable_nsfw) {
    safety_label_->setText(
        "NSFW mode is enabled. Blacklists and rating filters still apply.");
    safety_label_->setStyleSheet("color: #9a6700;");
    return;
  }
  safety_label_->setText(
      "SFW mode is active. NSFW providers are hidden, safe rating filters are "
      "forced where supported, and blacklisted tags are excluded.");
  safety_label_->setStyleSheet("color: #1f7a3a;");
}

}  // namespace boorubox::gui
