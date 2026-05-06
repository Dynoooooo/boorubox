#pragma once

#include <mutex>

#include "index/ArchiveIndex.hpp"

namespace boorubox {

class JsonlArchiveIndex final : public ArchiveIndex {
 public:
  explicit JsonlArchiveIndex(std::filesystem::path path);

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
  void load();
  void flush() const;

  std::filesystem::path path_;
  mutable std::mutex mutex_;
  std::vector<LocalArchiveItem> items_;
};

}  // namespace boorubox
