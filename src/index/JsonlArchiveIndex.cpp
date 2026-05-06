#include "index/JsonlArchiveIndex.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "util/PathUtil.hpp"
#include "util/StringUtil.hpp"

namespace boorubox {

namespace {

nlohmann::json to_json_object(const LocalArchiveItem& item) {
  return nlohmann::json{
      {"local_file_path", item.local_file_path.string()},
      {"provider", item.provider},
      {"post_id", item.post_id},
      {"post_url", item.post_url},
      {"file_url", item.file_url},
      {"thumbnail_path", item.thumbnail_path.string()},
      {"preview_path", item.preview_path.string()},
      {"hash", item.hash},
      {"tags", item.tags},
      {"artists", item.artists},
      {"rating", to_string(item.rating)},
      {"width", item.width},
      {"height", item.height},
      {"file_ext", item.file_ext},
      {"file_size", item.file_size},
      {"downloaded_at", item.downloaded_at},
      {"favorite", item.favorite},
      {"notes", item.notes},
  };
}

LocalArchiveItem from_json_object(const nlohmann::json& json) {
  LocalArchiveItem item;
  item.local_file_path = json.value("local_file_path", "");
  item.provider = json.value("provider", "");
  item.post_id = json.value("post_id", "");
  item.post_url = json.value("post_url", "");
  item.file_url = json.value("file_url", "");
  item.thumbnail_path = json.value("thumbnail_path", "");
  item.preview_path = json.value("preview_path", "");
  item.hash = json.value("hash", "");
  item.tags = json.value("tags", std::vector<std::string>{});
  item.artists = json.value("artists", std::vector<std::string>{});
  item.rating = rating_from_string(json.value("rating", "unknown"));
  item.width = json.value("width", 0);
  item.height = json.value("height", 0);
  item.file_ext = json.value("file_ext", "");
  item.file_size = json.value("file_size", static_cast<std::int64_t>(0));
  item.downloaded_at = json.value("downloaded_at", "");
  item.favorite = json.value("favorite", false);
  item.notes = json.value("notes", "");
  return item;
}

std::string now_iso8601() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&time, &tm);
  std::ostringstream out;
  out << std::put_time(&tm, "%FT%TZ");
  return out.str();
}

bool vector_contains_case_insensitive(const std::vector<std::string>& values,
                                      const std::string& needle) {
  if (needle.empty()) {
    return true;
  }
  const auto lowered = lower(needle);
  return std::ranges::any_of(values, [&](const std::string& value) {
    return lower(value).find(lowered) != std::string::npos;
  });
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
  if (!vector_contains_case_insensitive(item.tags, query.tag)) {
    return false;
  }
  if (!vector_contains_case_insensitive(item.artists, query.artist)) {
    return false;
  }
  return true;
}

bool is_image_like(const std::filesystem::path& path) {
  const auto ext = lower(path.extension().string());
  return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".gif" ||
         ext == ".webp" || ext == ".bmp";
}

}  // namespace

JsonlArchiveIndex::JsonlArchiveIndex(std::filesystem::path path)
    : path_(std::move(path)) {
  load();
}

void JsonlArchiveIndex::upsert(const LocalArchiveItem& item) {
  std::lock_guard lock(mutex_);
  auto match = std::ranges::find_if(items_, [&](const LocalArchiveItem& current) {
    const bool same_provider_post =
        !item.provider.empty() && !item.post_id.empty() &&
        current.provider == item.provider && current.post_id == item.post_id;
    const bool same_hash =
        !item.hash.empty() && !current.hash.empty() && current.hash == item.hash;
    const bool same_file =
        !item.file_url.empty() && !current.file_url.empty() &&
        current.file_url == item.file_url;
    return same_provider_post || same_hash || same_file;
  });
  if (match == items_.end()) {
    items_.push_back(item);
  } else {
    *match = item;
  }
  flush();
}

bool JsonlArchiveIndex::contains_duplicate(const std::string& hash,
                                           const std::string& provider,
                                           const std::string& post_id,
                                           const std::string& file_url) const {
  std::lock_guard lock(mutex_);
  return std::ranges::any_of(items_, [&](const LocalArchiveItem& item) {
    if (!hash.empty() && !item.hash.empty() && item.hash == hash) {
      return true;
    }
    if (!provider.empty() && !post_id.empty() && item.provider == provider &&
        item.post_id == post_id) {
      return true;
    }
    return !file_url.empty() && !item.file_url.empty() && item.file_url == file_url;
  });
}

std::optional<LocalArchiveItem> JsonlArchiveIndex::find_by_provider_id(
    const std::string& provider, const std::string& post_id) const {
  std::lock_guard lock(mutex_);
  const auto it = std::ranges::find_if(items_, [&](const LocalArchiveItem& item) {
    return item.provider == provider && item.post_id == post_id;
  });
  if (it == items_.end()) {
    return std::nullopt;
  }
  return *it;
}

std::optional<LocalArchiveItem> JsonlArchiveIndex::find_by_hash(
    const std::string& hash) const {
  if (hash.empty()) {
    return std::nullopt;
  }
  std::lock_guard lock(mutex_);
  const auto it = std::ranges::find_if(items_, [&](const LocalArchiveItem& item) {
    return item.hash == hash;
  });
  if (it == items_.end()) {
    return std::nullopt;
  }
  return *it;
}

std::vector<LocalArchiveItem> JsonlArchiveIndex::list(
    const ArchiveQuery& query) const {
  std::lock_guard lock(mutex_);
  std::vector<LocalArchiveItem> out;
  for (const auto& item : items_) {
    if (matches_query(item, query)) {
      out.push_back(item);
    }
  }
  return out;
}

void JsonlArchiveIndex::rebuild_from_directory(const std::filesystem::path& root) {
  if (!std::filesystem::exists(root)) {
    return;
  }
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(root)) {
    if (!entry.is_regular_file() || !is_image_like(entry.path())) {
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
    item.downloaded_at = now_iso8601();
    upsert(item);
  }
}

void JsonlArchiveIndex::load() {
  std::lock_guard lock(mutex_);
  items_.clear();
  std::ifstream file(path_);
  if (!file) {
    return;
  }
  std::string line;
  while (std::getline(file, line)) {
    line = trim(line);
    if (line.empty()) {
      continue;
    }
    try {
      items_.push_back(from_json_object(nlohmann::json::parse(line)));
    } catch (const std::exception&) {
      // Keep a partially damaged JSONL index usable by skipping bad rows.
    }
  }
}

void JsonlArchiveIndex::flush() const {
  ensure_directory(path_.parent_path());
  const auto temp_path = path_.string() + ".tmp";
  std::ofstream file(temp_path, std::ios::trunc);
  if (!file) {
    throw std::runtime_error("failed to write " + temp_path);
  }
  for (const auto& item : items_) {
    file << to_json_object(item).dump() << '\n';
  }
  file.close();
  std::filesystem::rename(temp_path, path_);
}

}  // namespace boorubox
