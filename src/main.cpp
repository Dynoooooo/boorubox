#include <exception>
#include <fstream>
#include <iostream>

#include <QApplication>
#include <QMessageBox>

#include "app/App.hpp"
#include "app/Config.hpp"
#include "gui/MainWindow.hpp"
#include "gui/QtUtil.hpp"
#include "util/PathUtil.hpp"

namespace {

bool acknowledge_nsfw_warning_gui(const boorubox::AppConfig& config,
                                  QWidget* parent = nullptr) {
  if (!config.enable_nsfw) {
    return true;
  }
  const auto ack_path = boorubox::nsfw_warning_ack_path();
  if (std::filesystem::exists(ack_path)) {
    return true;
  }

  const auto choice = QMessageBox::warning(
      parent, "NSFW Mode Enabled",
      "BooruBox NSFW mode is enabled.\n\n"
      "Only use providers and searches that are legal for you to access. "
      "Respect each site's rules, rate limits, and access controls. BooruBox "
      "will not assist with illegal sexual content, sexualized minors, "
      "non-consensual sexual content, or bypassing site controls.",
      QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
  if (choice != QMessageBox::Ok) {
    return false;
  }

  boorubox::ensure_directory(ack_path.parent_path());
  std::ofstream ack(ack_path);
  ack << "accepted\n";
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  QApplication qt_app(argc, argv);
  QApplication::setApplicationName("BooruBox");
  QApplication::setOrganizationName("BooruBox");

  try {
    const auto config_path = argc > 1 ? std::filesystem::path(argv[1])
                                      : boorubox::default_config_path();
    auto config = boorubox::Config::load(config_path);
    if (!acknowledge_nsfw_warning_gui(config)) {
      return 1;
    }

    boorubox::App app(std::move(config));
    boorubox::gui::MainWindow window(app, config_path);
    window.show();
    return QApplication::exec();
  } catch (const std::exception& error) {
    QMessageBox::critical(nullptr, "BooruBox Failed",
                          boorubox::gui::qs(error.what()));
    std::cerr << "boorubox: " << error.what() << '\n';
    return 1;
  }
}
