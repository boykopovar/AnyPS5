#include "prx/libSceNpTrophy2/include/TrophyNotice.hpp"

namespace NpTrophy2 {

std::string UnlockNoticeText(const TrophyPackage* package, std::int32_t trophyId) {
    std::string name;
    std::string title;
    if (package != nullptr) {
        title = package->title;
        if (const auto found = package->trophies.find(trophyId); found != package->trophies.end()) name = found->second.name;
    }
    if (name.empty()) name = "Trophy " + std::to_string(trophyId);
    if (title.empty()) return name;
    return name + " (" + title + ")";
}

}
