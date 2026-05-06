#pragma once

#include <QWidget>

namespace boorubox {
struct AppConfig;
}

namespace boorubox::gui {

bool confirm_nsfw_mode(QWidget* parent, const AppConfig& config);

}  // namespace boorubox::gui
