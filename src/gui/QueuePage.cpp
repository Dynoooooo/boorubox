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
  table_->setRowCount(static_cast<int>(jobs.size()));
  for (std::size_t i = 0; i < jobs.size(); ++i) {
    const auto& job = jobs[i];
    auto* job_item =
        new QTableWidgetItem(QString::number(static_cast<qulonglong>(job.id)));
    job_item->setData(Qt::UserRole,
                      QVariant::fromValue(static_cast<qulonglong>(job.id)));
    table_->setItem(static_cast<int>(i), 0, job_item);
    table_->setItem(static_cast<int>(i), 1,
                    new QTableWidgetItem(post_title(job.post)));
    table_->setItem(static_cast<int>(i), 2,
                    new QTableWidgetItem(download_status_label(job.status)));

    auto* progress = new QProgressBar(table_);
    progress->setRange(0, 100);
    const int percent =
        job.total_bytes > 0
            ? static_cast<int>((100 * job.bytes_downloaded) / job.total_bytes)
            : (job.status == DownloadStatus::Complete ? 100 : 0);
    progress->setValue(std::clamp(percent, 0, 100));
    progress->setTextVisible(true);
    table_->setCellWidget(static_cast<int>(i), 3, progress);

    table_->setItem(static_cast<int>(i), 4,
                    new QTableWidgetItem(speed_label(job.speed_bytes_per_second)));
    table_->setItem(static_cast<int>(i), 5,
                    new QTableWidgetItem(qs(job.error_message)));
  }
}

void QueuePage::setStatusCallback(std::function<void(QString)> callback) {
  status_callback_ = std::move(callback);
}

std::size_t QueuePage::selectedJobId() const {
  const auto row = table_->currentRow();
  if (row < 0) {
    return static_cast<std::size_t>(-1);
  }
  const auto* item = table_->item(row, 0);
  if (item == nullptr) {
    return static_cast<std::size_t>(-1);
  }
  return static_cast<std::size_t>(item->data(Qt::UserRole).toULongLong());
}

void QueuePage::retrySelected() {
  const auto job_id = selectedJobId();
  if (job_id == static_cast<std::size_t>(-1)) {
    return;
  }
  app_.retry_download(job_id);
  if (status_callback_) {
    status_callback_(QString("Retry requested for job %1")
                         .arg(static_cast<qulonglong>(job_id)));
  }
}

void QueuePage::cancelSelected() {
  const auto job_id = selectedJobId();
  if (job_id == static_cast<std::size_t>(-1)) {
    return;
  }
  app_.cancel_download(job_id);
  if (status_callback_) {
    status_callback_(QString("Cancel requested for job %1")
                         .arg(static_cast<qulonglong>(job_id)));
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
