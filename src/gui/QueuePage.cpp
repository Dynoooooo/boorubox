#include "gui/QueuePage.hpp"

#include <algorithm>

#include <QHeaderView>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QVariant>

#include "gui/QtUtil.hpp"

namespace boorubox::gui {

namespace {

constexpr int kColJobId = 0;
constexpr int kColPost = 1;
constexpr int kColStatus = 2;
constexpr int kColProgress = 3;
constexpr int kColSpeed = 4;
constexpr int kColError = 5;

QTableWidgetItem* ensure_item(QTableWidget* table, int row, int column) {
  auto* item = table->item(row, column);
  if (item == nullptr) {
    item = new QTableWidgetItem;
    table->setItem(row, column, item);
  }
  return item;
}

void set_item_text(QTableWidget* table, int row, int column,
                   const QString& text) {
  auto* item = ensure_item(table, row, column);
  if (item->text() != text) {
    item->setText(text);
  }
}

}  // namespace

QueuePage::QueuePage(App& app, QWidget* parent) : QWidget(parent), app_(app) {
  table_ = new QTableWidget(this);
  table_->setColumnCount(6);
  table_->setHorizontalHeaderLabels(
      {"Job", "Post", "Status", "Progress", "Speed", "Error"});
  table_->verticalHeader()->hide();
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->horizontalHeader()->setStretchLastSection(true);
  table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

  retry_button_ = new QPushButton("Retry Failed", this);
  cancel_button_ = new QPushButton("Cancel Selected", this);
  clear_failed_skipped_button_ =
      new QPushButton("Clear Failed/Skipped", this);

  auto* toolbar = new QHBoxLayout;
  toolbar->addStretch();
  toolbar->addWidget(clear_failed_skipped_button_);
  toolbar->addWidget(retry_button_);
  toolbar->addWidget(cancel_button_);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 14, 14, 14);
  layout->setSpacing(10);
  layout->addLayout(toolbar);
  layout->addWidget(table_, 1);

  timer_ = new QTimer(this);
  timer_->setInterval(800);
  connect(timer_, &QTimer::timeout, this, [this] { refresh(); });
  connect(retry_button_, &QPushButton::clicked, this, [this] { retrySelected(); });
  connect(cancel_button_, &QPushButton::clicked, this, [this] { cancelSelected(); });
  connect(clear_failed_skipped_button_, &QPushButton::clicked, this,
          [this] { clearFailedAndSkipped(); });
  timer_->start();
  refresh();
}

void QueuePage::refresh() {
  const auto jobs = app_.download_jobs();
  const int new_row_count = static_cast<int>(jobs.size());

  // Clear any cell widgets in rows that will be removed, so Qt does not keep
  // dangling child widgets around after setRowCount shrinks the table.
  for (int row = new_row_count; row < table_->rowCount(); ++row) {
    table_->removeCellWidget(row, kColProgress);
  }
  if (new_row_count != table_->rowCount()) {
    table_->setRowCount(new_row_count);
  }

  for (int row = 0; row < new_row_count; ++row) {
    const auto& job = jobs[static_cast<std::size_t>(row)];

    auto* job_item = ensure_item(table_, row, kColJobId);
    const QString job_id_text =
        QString::number(static_cast<qulonglong>(job.id));
    if (job_item->text() != job_id_text) {
      job_item->setText(job_id_text);
      job_item->setData(Qt::UserRole,
                        QVariant::fromValue(static_cast<qulonglong>(job.id)));
    }

    set_item_text(table_, row, kColPost, post_title(job.post));
    set_item_text(table_, row, kColStatus, download_status_label(job.status));

    auto* progress = qobject_cast<QProgressBar*>(
        table_->cellWidget(row, kColProgress));
    if (progress == nullptr) {
      progress = new QProgressBar(table_);
      progress->setRange(0, 100);
      progress->setTextVisible(true);
      table_->setCellWidget(row, kColProgress, progress);
    }
    const int percent =
        job.total_bytes > 0
            ? static_cast<int>((100 * job.bytes_downloaded) / job.total_bytes)
            : (job.status == DownloadStatus::Complete ? 100 : 0);
    const int clamped = std::clamp(percent, 0, 100);
    if (progress->value() != clamped) {
      progress->setValue(clamped);
    }

    set_item_text(table_, row, kColSpeed, speed_label(job.speed_bytes_per_second));
    set_item_text(table_, row, kColError, qs(job.error_message));
  }
}

void QueuePage::setStatusCallback(std::function<void(QString)> callback) {
  status_callback_ = std::move(callback);
}

std::optional<std::size_t> QueuePage::selectedJobId() const {
  const auto row = table_->currentRow();
  if (row < 0) {
    return std::nullopt;
  }
  const auto* item = table_->item(row, kColJobId);
  if (item == nullptr) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(item->data(Qt::UserRole).toULongLong());
}

void QueuePage::retrySelected() {
  const auto job_id = selectedJobId();
  if (!job_id) {
    return;
  }
  app_.retry_download(*job_id);
  if (status_callback_) {
    status_callback_(QString("Retry requested for job %1")
                         .arg(static_cast<qulonglong>(*job_id)));
  }
}

void QueuePage::cancelSelected() {
  const auto job_id = selectedJobId();
  if (!job_id) {
    return;
  }
  app_.cancel_download(*job_id);
  if (status_callback_) {
    status_callback_(QString("Cancel requested for job %1")
                         .arg(static_cast<qulonglong>(*job_id)));
  }
}

void QueuePage::clearFailedAndSkipped() {
  const auto cleared = app_.clear_failed_and_skipped_downloads();
  refresh();
  if (status_callback_) {
    status_callback_(QString("Cleared %1 failed/skipped download job(s)")
                         .arg(static_cast<qulonglong>(cleared)));
  }
}

}  // namespace boorubox::gui
