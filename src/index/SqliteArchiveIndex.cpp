#include "index/SqliteArchiveIndex.hpp"

#include <algorithm>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "util/PathUtil.hpp"
#include "util/StringUtil.hpp"

#ifdef BOORUBOX_WITH_SQLITE
#include <sqlite3.h>
#endif

namespace boorubox {

namespace {

std::string to_json_array(const std::vector<std::string>& values) {
  return nlohmann::json(values).dump();
}

std::vector<std::string> from_json_array(const unsigned char* text) {
  if (text == nullptr) {
    return {};
  }
  try {
    return nlohmann::json::parse(reinterpret_cast<const char*>(text))
        .get<std::vector<std::string>>();
  } catch (const std::exception&) {
    return {};
  }
}

bool matches_query(const LocalArchiveItem& item, const ArchiveQuery& query) {
  if (!query.provider.empty() && item.provider != query.provider) {
    return false;
  }
  if (query.rating != Rating::Unknown && item.rating != query.rating) {
    return false;
  }
  if (query.favorites_only && !item.favorite) {
    return false;
  }
  return contains_substring_ci(item.tags, query.tag) &&
         contains_substring_ci(item.artists, query.artist);
}

#ifdef BOORUBOX_WITH_SQLITE

void check_sqlite(int code, sqlite3* db, std::string_view context) {
  if (code != SQLITE_OK && code != SQLITE_DONE && code != SQLITE_ROW) {
    throw std::runtime_error(std::string(context) + ": " + sqlite3_errmsg(db));
  }
}

class Statement {
 public:
  Statement(sqlite3* db, const char* sql) : db_(db) {
    check_sqlite(sqlite3_prepare_v2(db_, sql, -1, &stmt_, nullptr), db_,
                 "prepare sqlite statement");
  }

  ~Statement() {
    sqlite3_finalize(stmt_);
  }

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  sqlite3_stmt* get() const {
    return stmt_;
  }

  void bind(int index, const std::string& value) {
    check_sqlite(sqlite3_bind_text(stmt_, index, value.c_str(), -1,
                                   SQLITE_TRANSIENT),
                 db_, "bind text");
  }

  void bind(int index, std::int64_t value) {
    check_sqlite(sqlite3_bind_int64(stmt_, index, value), db_, "bind int64");
  }

  void bind(int index, int value) {
    check_sqlite(sqlite3_bind_int(stmt_, index, value), db_, "bind int");
  }

 private:
  sqlite3* db_ = nullptr;
  sqlite3_stmt* stmt_ = nullptr;
};

class Transaction {
 public:
  explicit Transaction(sqlite3* db) : db_(db) {
    check_sqlite(sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr),
                 db_, "begin sqlite transaction");
  }

  ~Transaction() {
    if (active_) {
      sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
    }
  }

  Transaction(const Transaction&) = delete;
  Transaction& operator=(const Transaction&) = delete;

  void commit() {
    check_sqlite(sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr),
                 db_, "commit sqlite transaction");
    active_ = false;
  }

 private:
  sqlite3* db_ = nullptr;
  bool active_ = true;
};

std::string column_text(sqlite3_stmt* stmt, int column) {
  const auto* text = sqlite3_column_text(stmt, column);
  return text == nullptr ? std::string{} : reinterpret_cast<const char*>(text);
}

LocalArchiveItem row_to_item(sqlite3_stmt* stmt) {
  LocalArchiveItem item;
  item.local_file_path = column_text(stmt, 0);
  item.provider = column_text(stmt, 1);
  item.post_id = column_text(stmt, 2);
  item.post_url = column_text(stmt, 3);
  item.file_url = column_text(stmt, 4);
  item.thumbnail_path = column_text(stmt, 5);
  item.preview_path = column_text(stmt, 6);
  item.hash = column_text(stmt, 7);
  item.tags = from_json_array(sqlite3_column_text(stmt, 8));
  item.artists = from_json_array(sqlite3_column_text(stmt, 9));
  item.rating = rating_from_string(column_text(stmt, 10));
  item.width = sqlite3_column_int(stmt, 11);
  item.height = sqlite3_column_int(stmt, 12);
  item.file_ext = column_text(stmt, 13);
  item.file_size = sqlite3_column_int64(stmt, 14);
  item.downloaded_at = column_text(stmt, 15);
  item.favorite = sqlite3_column_int(stmt, 16) != 0;
  item.notes = column_text(stmt, 17);
  return item;
}

constexpr const char* kSelectColumns =
    "local_file_path, provider, post_id, post_url, file_url, thumbnail_path, "
    "preview_path, hash, tags_json, artists_json, rating, width, height, "
    "file_ext, file_size, downloaded_at, favorite, notes";

#endif

}  // namespace

class SqliteArchiveIndex::Impl {
 public:
#ifdef BOORUBOX_WITH_SQLITE
  explicit Impl(const std::filesystem::path& path) {
    ensure_directory(path.parent_path());
    if (sqlite3_open(path.string().c_str(), &db_) != SQLITE_OK) {
      const std::string message = sqlite3_errmsg(db_);
      sqlite3_close(db_);
      db_ = nullptr;
      throw std::runtime_error("open sqlite index: " + message);
    }
    sqlite3_busy_timeout(db_, 5000);
    exec(
        "CREATE TABLE IF NOT EXISTS archive_items ("
        "id INTEGER PRIMARY KEY,"
        "local_file_path TEXT NOT NULL,"
        "provider TEXT NOT NULL,"
        "post_id TEXT NOT NULL,"
        "post_url TEXT,"
        "file_url TEXT,"
        "thumbnail_path TEXT,"
        "preview_path TEXT,"
        "hash TEXT,"
        "tags_json TEXT NOT NULL,"
        "artists_json TEXT NOT NULL,"
        "rating TEXT,"
        "width INTEGER,"
        "height INTEGER,"
        "file_ext TEXT,"
        "file_size INTEGER,"
        "downloaded_at TEXT,"
        "favorite INTEGER NOT NULL DEFAULT 0,"
        "notes TEXT"
        ")");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_archive_provider_post "
         "ON archive_items(provider, post_id)");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_archive_hash "
         "ON archive_items(hash) WHERE hash <> ''");
    exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_archive_file_url "
         "ON archive_items(file_url) WHERE file_url <> ''");
  }

  ~Impl() {
    if (db_ != nullptr) {
      sqlite3_close(db_);
    }
  }

  void exec(const char* sql) const {
    char* error = nullptr;
    const auto code = sqlite3_exec(db_, sql, nullptr, nullptr, &error);
    if (code != SQLITE_OK) {
      std::string message = error == nullptr ? sqlite3_errmsg(db_) : error;
      sqlite3_free(error);
      throw std::runtime_error(message);
    }
  }

  sqlite3* db() const {
    return db_;
  }

 private:
  sqlite3* db_ = nullptr;
#else
  explicit Impl(const std::filesystem::path&) {
    throw std::runtime_error(
        "SqliteArchiveIndex was not built because SQLite was not found by CMake");
  }
#endif
};

SqliteArchiveIndex::SqliteArchiveIndex(const std::filesystem::path& path)
    : impl_(std::make_unique<Impl>(path)) {}

SqliteArchiveIndex::~SqliteArchiveIndex() = default;

void SqliteArchiveIndex::upsert(const LocalArchiveItem& item) {
#ifdef BOORUBOX_WITH_SQLITE
  std::lock_guard lock(mutex_);
  Transaction transaction(impl_->db());
  {
    Statement delete_duplicates(
        impl_->db(),
        "DELETE FROM archive_items WHERE "
        "((hash = ? AND ? <> '') OR "
        "(provider = ? AND post_id = ? AND ? <> '') OR "
        "(file_url = ? AND ? <> ''))");
    delete_duplicates.bind(1, item.hash);
    delete_duplicates.bind(2, item.hash);
    delete_duplicates.bind(3, item.provider);
    delete_duplicates.bind(4, item.post_id);
    delete_duplicates.bind(5, item.post_id);
    delete_duplicates.bind(6, item.file_url);
    delete_duplicates.bind(7, item.file_url);
    check_sqlite(sqlite3_step(delete_duplicates.get()), impl_->db(),
                 "delete duplicate archive rows");
  }

  Statement insert(
      impl_->db(),
      "INSERT INTO archive_items ("
      "local_file_path, provider, post_id, post_url, file_url, thumbnail_path, "
      "preview_path, hash, tags_json, artists_json, rating, width, height, "
      "file_ext, file_size, downloaded_at, favorite, notes"
      ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
  insert.bind(1, item.local_file_path.string());
  insert.bind(2, item.provider);
  insert.bind(3, item.post_id);
  insert.bind(4, item.post_url);
  insert.bind(5, item.file_url);
  insert.bind(6, item.thumbnail_path.string());
  insert.bind(7, item.preview_path.string());
  insert.bind(8, item.hash);
  insert.bind(9, to_json_array(item.tags));
  insert.bind(10, to_json_array(item.artists));
  insert.bind(11, to_string(item.rating));
  insert.bind(12, item.width);
  insert.bind(13, item.height);
  insert.bind(14, item.file_ext);
  insert.bind(15, item.file_size);
  insert.bind(16, item.downloaded_at);
  insert.bind(17, item.favorite ? 1 : 0);
  insert.bind(18, item.notes);
  check_sqlite(sqlite3_step(insert.get()), impl_->db(), "insert archive row");
  transaction.commit();
#else
  (void)item;
#endif
}

bool SqliteArchiveIndex::contains_duplicate(const std::string& hash,
                                            const std::string& provider,
                                            const std::string& post_id,
                                            const std::string& file_url) const {
#ifdef BOORUBOX_WITH_SQLITE
  std::lock_guard lock(mutex_);
  Statement stmt(
      impl_->db(),
      "SELECT 1 FROM archive_items WHERE "
      "((hash = ? AND ? <> '') OR "
      "(provider = ? AND post_id = ? AND ? <> '') OR "
      "(file_url = ? AND ? <> '')) LIMIT 1");
  stmt.bind(1, hash);
  stmt.bind(2, hash);
  stmt.bind(3, provider);
  stmt.bind(4, post_id);
  stmt.bind(5, post_id);
  stmt.bind(6, file_url);
  stmt.bind(7, file_url);
  return sqlite3_step(stmt.get()) == SQLITE_ROW;
#else
  (void)hash;
  (void)provider;
  (void)post_id;
  (void)file_url;
  return false;
#endif
}

std::optional<LocalArchiveItem> SqliteArchiveIndex::find_by_provider_id(
    const std::string& provider, const std::string& post_id) const {
#ifdef BOORUBOX_WITH_SQLITE
  std::lock_guard lock(mutex_);
  Statement stmt(impl_->db(),
                 (std::string("SELECT ") + kSelectColumns +
                  " FROM archive_items WHERE provider = ? AND post_id = ? LIMIT 1")
                     .c_str());
  stmt.bind(1, provider);
  stmt.bind(2, post_id);
  if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
    return std::nullopt;
  }
  return row_to_item(stmt.get());
#else
  (void)provider;
  (void)post_id;
  return std::nullopt;
#endif
}

std::optional<LocalArchiveItem> SqliteArchiveIndex::find_by_hash(
    const std::string& hash) const {
#ifdef BOORUBOX_WITH_SQLITE
  std::lock_guard lock(mutex_);
  Statement stmt(impl_->db(),
                 (std::string("SELECT ") + kSelectColumns +
                  " FROM archive_items WHERE hash = ? AND ? <> '' LIMIT 1")
                     .c_str());
  stmt.bind(1, hash);
  stmt.bind(2, hash);
  if (sqlite3_step(stmt.get()) != SQLITE_ROW) {
    return std::nullopt;
  }
  return row_to_item(stmt.get());
#else
  (void)hash;
  return std::nullopt;
#endif
}

std::vector<LocalArchiveItem> SqliteArchiveIndex::list(
    const ArchiveQuery& query) const {
#ifdef BOORUBOX_WITH_SQLITE
  std::lock_guard lock(mutex_);
  Statement stmt(impl_->db(),
                 (std::string("SELECT ") + kSelectColumns +
                  " FROM archive_items ORDER BY downloaded_at DESC")
                     .c_str());
  std::vector<LocalArchiveItem> out;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    auto item = row_to_item(stmt.get());
    if (matches_query(item, query)) {
      out.push_back(std::move(item));
    }
  }
  return out;
#else
  (void)query;
  return {};
#endif
}

void SqliteArchiveIndex::rebuild_from_directory(
    const std::filesystem::path& root) {
  if (!std::filesystem::exists(root)) {
    return;
  }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file() || !is_image_file(entry.path())) {
      continue;
    }
    LocalArchiveItem item;
    item.local_file_path = entry.path();
    item.provider = "local";
    item.post_id = entry.path().filename().string();
    item.file_ext = lower(entry.path().extension().string());
    if (!item.file_ext.empty() && item.file_ext.front() == '.') {
      item.file_ext.erase(item.file_ext.begin());
    }
    item.file_size = static_cast<std::int64_t>(entry.file_size());
    item.downloaded_at = now_iso8601_utc();
    upsert(item);
  }
}

}  // namespace boorubox
