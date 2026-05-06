#pragma once

#include <filesystem>
#include <memory>
#include <mutex>

#include "index/ArchiveIndex.hpp"

namespace boorubox {

class SqliteArchiveIndex final : public ArchiveIndex {
 public:
  explicit SqliteArchiveIndex(const std::filesystem::path& path);
  ~SqliteArchiveIndex() override;

  SqliteArchiveIndex(const SqliteArchiveIndex&) = delete;
  SqliteArchiveIndex& operator=(const SqliteArchiveIndex&) = delete;

  void upsert(const LocalArchiveItem& item) override;
  bool contains_duplicate(const std::string& hash, const std::string& provider,
                          const std::string& post_id,
                          const std::string& file_url) const override;
  std::optional<LocalArchiveItem> find_by_provider_id(
      const std::string& provider, const std::string& post_id) const override;
  std::optional<LocalArchiveItem> find_by_hash(
      const std::string& hash) const override;
  std::vector<LocalArchiveItem> list(const ArchiveQuery& query = {}) const override;
  void rebuild_from_directory(const std::filesystem::path& root) override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
  mutable std::mutex mutex_;
};

}  // namespace boorubox
