#include "prx/libSceNpTrophy2/include/TrophyNotice.hpp"
#include "prx/libSceNpTrophy2/include/TrophyPackage.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

namespace {

bool correct = true;

void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "trophy notice: %s\n", message);
        correct = false;
    }
}

NpTrophy2::TrophyPackage Package() {
    NpTrophy2::TrophyPackage package;
    package.title = "Demo Game";
    NpTrophy2::TrophyDefinition first;
    first.id = 1;
    first.name = "First Steps";
    package.trophies.emplace(first.id, first);
    NpTrophy2::TrophyDefinition unnamed;
    unnamed.id = 2;
    package.trophies.emplace(unnamed.id, unnamed);
    return package;
}

}

int main() {
    const auto package = Package();
    check(NpTrophy2::UnlockNoticeText(&package, 1) == "First Steps (Demo Game)", "a named trophy must show its name and the title");
    check(NpTrophy2::UnlockNoticeText(&package, 2) == "Trophy 2 (Demo Game)", "an unnamed trophy must fall back to its id");
    check(NpTrophy2::UnlockNoticeText(&package, 9) == "Trophy 9 (Demo Game)", "an unknown trophy must fall back to its id");
    check(NpTrophy2::UnlockNoticeText(nullptr, 9) == "Trophy 9", "without a package the notice must still name the trophy");
    NpTrophy2::TrophyPackage untitled = package;
    untitled.title.clear();
    check(NpTrophy2::UnlockNoticeText(&untitled, 1) == "First Steps", "an empty title must not add brackets");
    return correct ? 0 : 1;
}
