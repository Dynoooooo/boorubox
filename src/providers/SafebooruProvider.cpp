#include "providers/SafebooruProvider.hpp"

namespace boorubox {

SafebooruProvider::SafebooruProvider(std::string base_url)
    : GelbooruProvider("safebooru", std::move(base_url), false, 1000, "", "") {}

}  // namespace boorubox
