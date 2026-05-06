#include "gui/SafetyUtil.hpp"

#include <fstream>

#include <QMessageBox>

#include "app/Config.hpp"
#include "util/PathUtil.hpp"

namespace boorubox::gui {

bool confirm_nsfw_mode(QWidget* parent, const AppConfig& config) {
  if (config.enable_nsfw) {
    return true;
  }

  const auto ack_path = nsfw_warning_ack_path();
  if (std::filesystem::exists(ack_path)) {
    return true;
  }

  const auto choice = QMessageBox::warning(
      parent, "Turn Off SFW Mode",
      "Turning off SFW mode can reveal NSFW providers and rating filters.\n\n"
      "Only use providers and searches that are legal for you to access. "
      "Respect each site's rules, rate limits, and access controls. BooruBox "
      "will not assist with illegal sexual content, sexualized minors, "
      "non-consensual sexual content, content that violates site rules, or "
      "bypassing site controls.\n\n"
      "Blacklists and rating filters still apply.",
      QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);

  if (choice != QMessageBox::Ok) {
    return false;
  }

  ensure_directory(ack_path.parent_path());
  std::ofstream ack(ack_path);
  ack << "accepted\n";
  return true;
}

}  // namespace boorubox::gui
