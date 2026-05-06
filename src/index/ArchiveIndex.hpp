#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "model/LocalArchiveItem.hpp"

namespace boorubox {

struct ArchiveQuery {
  std::string provider;
  Rating rating = Rating::Unknown;
  std::string tag;
  std::string artist;
  bool favorites_only = false;
};

class ArchiveIndex {
 public:
  virtual ~ArchiveIndex() = default;

  virtual void upsert(const LocalArchiveItem& item) = 0;
  virtual bool contains_duplicate(const std::string& hash,
                                  const std::string& provider,
                                  const std::string& post_id,
                                  const std::string& file_url) const = 0;
  virtual std::optional<LocalArchiveItem> find_by_provider_id(
      const std::string& provider, const std::string& post_id) const = 0;
  virtual std::optional<LocalArchiveItem> find_by_hash(
      const std::string& hash) const = 0;
  virtual std::vector<LocalArchiveItem> list(
      const ArchiveQuery& query = {}) const = 0;
  virtual void rebuild_from_directory(const std::filesystem::path& root) = 0;
};

}  // namespace boorubox
