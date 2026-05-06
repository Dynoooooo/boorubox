#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/dom/elements.hpp>

#include "app/App.hpp"
#include "preview/ImagePreviewBackend.hpp"
#include "tui/Screens.hpp"

namespace boorubox::tui {

class TuiApp {
 public:
  explicit TuiApp(App& app);
  void run();

 private:
  ftxui::Element render_search();
  ftxui::Element render_results();
  ftxui::Element render_queue();
  ftxui::Element render_gallery();
  ftxui::Element render_settings();
  ftxui::Element render_logs();
  ftxui::Element render_footer() const;
  void do_search();
  void download_selected();
  void preview_selected();

  App& app_;
  ScreenId screen_ = ScreenId::Search;
  std::vector<std::string> providers_;
  int provider_index_ = 0;
  std::string tags_;
  std::string excluded_tags_;
  std::string status_;
  std::vector<Post> results_;
  int result_index_ = 0;
  std::vector<LocalArchiveItem> gallery_;
  int gallery_index_ = 0;
  std::unique_ptr<ImagePreviewBackend> preview_backend_;
};

}  // namespace boorubox::tui
