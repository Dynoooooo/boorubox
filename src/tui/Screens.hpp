#pragma once

#include <string>

namespace boorubox::tui {

enum class ScreenId {
  Search,
  Results,
  Queue,
  Gallery,
  Settings,
  Logs,
};

std::string screen_title(ScreenId screen);

}  // namespace boorubox::tui
