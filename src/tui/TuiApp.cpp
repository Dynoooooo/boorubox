#include "tui/TuiApp.hpp"

#include <algorithm>
#include <memory>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "preview/BlockPreview.hpp"
#include "preview/KittyPreview.hpp"
#include "preview/SixelPreview.hpp"
#include "util/StringUtil.hpp"

namespace boorubox::tui {

namespace {

using namespace ftxui;

std::string dimensions(const Post& post) {
  if (post.width <= 0 || post.height <= 0) {
    return "?x?";
  }
  return std::to_string(post.width) + "x" + std::to_string(post.height);
}

std::string dimensions(const LocalArchiveItem& item) {
  if (item.width <= 0 || item.height <= 0) {
    return "?x?";
  }
  return std::to_string(item.width) + "x" + std::to_string(item.height);
}

Element labeled(std::string label, Element value) {
  return hbox({text(label) | color(Color::GrayLight), value});
}

std::unique_ptr<ImagePreviewBackend> make_preview_backend(std::string preferred) {
  preferred = lower(preferred);
  if (preferred == "kitty") {
    auto backend = std::make_unique<KittyPreview>();
    if (backend->is_supported()) {
      return backend;
    }
  }
  if (preferred == "sixel") {
    auto backend = std::make_unique<SixelPreview>();
    if (backend->is_supported()) {
      return backend;
    }
  }
  auto kitty = std::make_unique<KittyPreview>();
  if (kitty->is_supported()) {
    return kitty;
  }
  auto sixel = std::make_unique<SixelPreview>();
  if (sixel->is_supported()) {
    return sixel;
  }
  return std::make_unique<BlockPreview>();
}

}  // namespace

std::string screen_title(ScreenId screen) {
  switch (screen) {
    case ScreenId::Search:
      return "Search";
    case ScreenId::Results:
      return "Results";
    case ScreenId::Queue:
      return "Queue";
    case ScreenId::Gallery:
      return "Gallery";
    case ScreenId::Settings:
      return "Settings";
    case ScreenId::Logs:
      return "Logs";
  }
  return "BooruBox";
}

TuiApp::TuiApp(App& app)
    : app_(app),
      providers_(app.provider_names()),
      preview_backend_(make_preview_backend(app.config().preferred_preview_backend)) {
  if (providers_.empty()) {
    providers_.push_back("no providers enabled");
  }
}

void TuiApp::run() {
  auto screen = ScreenInteractive::Fullscreen();

  auto provider_menu = Radiobox(&providers_, &provider_index_);
  auto tags_input = Input(&tags_, "tags");
  auto excluded_input = Input(&excluded_tags_, "excluded tags");
  auto search_button = Button("Search", [&] { do_search(); });
  auto search_controls = Container::Vertical({
      provider_menu,
      tags_input,
      excluded_input,
      search_button,
  });

  auto renderer = Renderer(search_controls, [&] {
    Element body;
    switch (screen_) {
      case ScreenId::Search:
        body = hbox({
                   vbox({
                       text("BooruBox") | bold,
                       separator(),
                       text("Provider"),
                       provider_menu->Render(),
                       separator(),
                       text("Tags"),
                       tags_input->Render(),
                       text("Excluded tags"),
                       excluded_input->Render(),
                       separator(),
                       search_button->Render(),
                   }) | border | size(WIDTH, EQUAL, 38),
                   render_search() | flex,
               }) |
               flex;
        break;
      case ScreenId::Results:
        body = render_results();
        break;
      case ScreenId::Queue:
        body = render_queue();
        break;
      case ScreenId::Gallery:
        gallery_ = app_.archive_items();
        body = render_gallery();
        break;
      case ScreenId::Settings:
        body = render_settings();
        break;
      case ScreenId::Logs:
        body = render_logs();
        break;
    }
    return vbox({body | flex, render_footer()});
  });

  renderer |= CatchEvent([&](Event event) {
    if (event == Event::Character('q')) {
      if (screen_ == ScreenId::Search) {
        screen.ExitLoopClosure()();
      } else {
        screen_ = ScreenId::Search;
      }
      return true;
    }
    if (event == Event::Character('/')) {
      screen_ = ScreenId::Search;
      return true;
    }
    if (event == Event::Character('g')) {
      screen_ = ScreenId::Gallery;
      return true;
    }
    if (event == Event::Character('?')) {
      screen_ = ScreenId::Settings;
      return true;
    }
    if (event == Event::Character('d')) {
      download_selected();
      return true;
    }
    if (event == Event::Character('a')) {
      for (const auto& post : results_) {
        app_.enqueue_download(post);
      }
      app_.start_downloads();
      status_ = "queued visible results";
      screen_ = ScreenId::Queue;
      return true;
    }
    if (event == Event::Character('p')) {
      preview_selected();
      return true;
    }
    if (event == Event::Tab) {
      screen_ = static_cast<ScreenId>((static_cast<int>(screen_) + 1) % 6);
      return true;
    }
    if (event == Event::ArrowDown && screen_ == ScreenId::Results) {
      result_index_ = std::min(result_index_ + 1,
                               static_cast<int>(std::max<std::size_t>(1, results_.size())) - 1);
      return true;
    }
    if (event == Event::ArrowUp && screen_ == ScreenId::Results) {
      result_index_ = std::max(result_index_ - 1, 0);
      return true;
    }
    if (event == Event::ArrowDown && screen_ == ScreenId::Gallery) {
      gallery_index_ = std::min(gallery_index_ + 1,
                                static_cast<int>(std::max<std::size_t>(1, gallery_.size())) - 1);
      return true;
    }
    if (event == Event::ArrowUp && screen_ == ScreenId::Gallery) {
      gallery_index_ = std::max(gallery_index_ - 1, 0);
      return true;
    }
    return false;
  });

  screen.Loop(renderer);
}

Element TuiApp::render_search() {
  return hbox({
             vbox({
                 text("BooruBox") | bold,
                 separator(),
                 text("Provider"),
                 text(providers_[provider_index_]) | color(Color::Cyan),
                 separator(),
                 labeled("Tags: ", text(tags_.empty() ? " " : tags_)),
                 labeled("Exclude: ", text(excluded_tags_.empty() ? " " : excluded_tags_)),
                 separator(),
                 text("Press Enter on Search, or / to return here."),
             }) | border | size(WIDTH, EQUAL, 36),
             vbox({
                 text("Safety") | bold,
                 separator(),
                 labeled("enable_nsfw: ",
                         text(app_.config().enable_nsfw ? "true" : "false")),
                 labeled("default rating: ", text(to_string(app_.config().default_rating))),
                 labeled("preview backend: ", text(preview_backend_->name())),
                 separator(),
                 paragraph("NSFW providers are hidden when enable_nsfw=false. "
                           "Searches add safe rating filters and blacklist exclusions "
                           "where providers support them."),
             }) | border | flex,
         }) |
         flex;
}

Element TuiApp::render_results() {
  std::vector<Element> rows;
  for (std::size_t i = 0; i < results_.size(); ++i) {
    const auto& post = results_[i];
    auto row = hbox({
        text(i == static_cast<std::size_t>(result_index_) ? ">" : " "),
        text(post.provider) | size(WIDTH, EQUAL, 10),
        text(post.id) | size(WIDTH, EQUAL, 10),
        text(to_string(post.rating)) | size(WIDTH, EQUAL, 12),
        text(dimensions(post)) | size(WIDTH, EQUAL, 12),
        text(std::to_string(post.score)) | size(WIDTH, EQUAL, 8),
        text(join(std::vector<std::string>(
                      post.tags.begin(),
                      post.tags.begin() + std::min<std::size_t>(post.tags.size(), 5)),
                  " ")),
    });
    rows.push_back(row);
  }
  if (rows.empty()) {
    rows.push_back(text("No results yet."));
  }

  Element details = text("No selection");
  if (!results_.empty()) {
    const auto& post = results_[result_index_];
    details = vbox({
        text("Details") | bold,
        separator(),
        labeled("Post: ", text(post.post_url)),
        labeled("File: ", text(post.file_ext + " " + dimensions(post))),
        labeled("Rating: ", text(to_string(post.rating))),
        labeled("Artists: ", text(join(post.artist_tags, " "))),
        labeled("Preview: ", text(post.preview_url.empty() ? "none" : post.preview_url)),
    });
  }

  return hbox({
             vbox(rows) | border | flex,
             details | border | size(WIDTH, GREATER_THAN, 42),
         }) |
         flex;
}

Element TuiApp::render_queue() {
  std::vector<Element> rows;
  for (const auto& job : app_.download_jobs()) {
    const auto ratio = job.total_bytes > 0
                           ? static_cast<float>(job.bytes_downloaded) /
                                 static_cast<float>(job.total_bytes)
                           : 0.0f;
    rows.push_back(vbox({
        hbox({
            text(job.post.provider + "/" + job.post.id) | size(WIDTH, EQUAL, 24),
            text(to_string(job.status)) | size(WIDTH, EQUAL, 12),
            text(std::to_string(static_cast<int>(job.speed_bytes_per_second / 1024)) +
                 " KiB/s"),
        }),
        gauge(ratio),
        text(job.error_message) | color(Color::Red),
    }));
  }
  if (rows.empty()) {
    rows.push_back(text("Queue is empty."));
  }
  return vbox(rows) | border | flex;
}

Element TuiApp::render_gallery() {
  std::vector<Element> rows;
  for (std::size_t i = 0; i < gallery_.size(); ++i) {
    const auto& item = gallery_[i];
    rows.push_back(hbox({
        text(i == static_cast<std::size_t>(gallery_index_) ? ">" : " "),
        text(item.provider) | size(WIDTH, EQUAL, 10),
        text(item.post_id) | size(WIDTH, EQUAL, 12),
        text(to_string(item.rating)) | size(WIDTH, EQUAL, 12),
        text(dimensions(item)) | size(WIDTH, EQUAL, 12),
        text(item.local_file_path.string()),
    }));
  }
  if (rows.empty()) {
    rows.push_back(text("No local archive items yet."));
  }
  return vbox(rows) | border | flex;
}

Element TuiApp::render_settings() {
  const auto& config = app_.config();
  return vbox({
             text("Settings") | bold,
             separator(),
             labeled("download_dir: ", text(config.download_dir.string())),
             labeled("cache_dir: ", text(config.cache_dir.string())),
             labeled("index_path: ", text(config.index_path.string())),
             labeled("user_agent: ", text(config.user_agent)),
             labeled("max_concurrent_downloads: ",
                     text(std::to_string(config.max_concurrent_downloads))),
             labeled("enable_nsfw: ", text(config.enable_nsfw ? "true" : "false")),
             labeled("blacklisted_tags: ", text(join(config.blacklisted_tags, ", "))),
         }) |
         border | flex;
}

Element TuiApp::render_logs() {
  std::vector<Element> rows;
  for (const auto& line : app_.logs()) {
    rows.push_back(text(line));
  }
  if (rows.empty()) {
    rows.push_back(text("No logs yet."));
  }
  return vbox(rows) | border | flex;
}

Element TuiApp::render_footer() const {
  return text(" / search  Tab next  p preview  d download  a queue all  g gallery  ? settings  q back/quit " +
              status_) |
         color(Color::GrayLight);
}

void TuiApp::do_search() {
  try {
    SearchQuery query;
    query.provider_name = providers_[provider_index_];
    query.tags = split_words(tags_);
    query.excluded_tags = split_words(excluded_tags_);
    query.rating_filter = app_.config().default_rating;
    query.limit = 20;
    query.page = 1;
    results_ = app_.search(query);
    result_index_ = 0;
    status_ = "found " + std::to_string(results_.size()) + " result(s)";
    screen_ = ScreenId::Results;
  } catch (const std::exception& error) {
    status_ = error.what();
    screen_ = ScreenId::Logs;
  }
}

void TuiApp::download_selected() {
  if (results_.empty()) {
    status_ = "no selected result";
    return;
  }
  try {
    app_.enqueue_download(results_[result_index_]);
    app_.start_downloads();
    status_ = "queued " + results_[result_index_].provider + "/" +
              results_[result_index_].id;
    screen_ = ScreenId::Queue;
  } catch (const std::exception& error) {
    status_ = error.what();
  }
}

void TuiApp::preview_selected() {
  if (results_.empty()) {
    status_ = "no selected result";
    return;
  }
  try {
    const auto path = app_.ensure_preview(results_[result_index_]);
    if (path.empty()) {
      status_ = "post has no preview URL";
      return;
    }
    preview_backend_->render_image(path, PreviewBounds{.columns = 42, .rows = 20});
    status_ = "preview cached at " + path.string();
  } catch (const std::exception& error) {
    status_ = error.what();
  }
}

}  // namespace boorubox::tui
