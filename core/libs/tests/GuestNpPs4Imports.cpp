#include "prx/libSceNpTrophy/include/Api.hpp"
#include "prx/libSceNpSignaling/include/Api.hpp"
#include "prx/libSceNpMatching2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr int Matching2AlreadyInitialized = static_cast<int>(0x80550C02u);
constexpr int Matching2NotInitialized = static_cast<int>(0x80550C03u);
constexpr int Matching2InvalidArgument = static_cast<int>(0x80550C0Au);
constexpr int TrophyInvalidArgument = static_cast<int>(0x80551604u);
constexpr int TrophyInvalidTrophyId = static_cast<int>(0x8055160au);
constexpr int TrophyInvalidGroupId = static_cast<int>(0x8055160bu);
constexpr int TrophyNotRegistered = static_cast<int>(0x8055160fu);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpPs4Imports: %s\n", message);
        std::abort();
    }
}

void WriteBigEndian(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint64_t value, std::size_t width) {
    for (std::size_t index = 0; index < width; ++index) bytes[at + index] = static_cast<std::uint8_t>(value >> (8u * (width - 1u - index)));
}

void WriteTrophyPackage(const std::filesystem::path& path) {
    const char* const names[] = {"TROPCONF.ESFM", "TROP000.PNG", "TROP001.PNG", "TROP002.PNG", "ICON0.PNG"};
    constexpr std::size_t count = sizeof(names) / sizeof(names[0]);
    std::vector<std::uint8_t> bytes(0x60 + count * 0x40 + count * 16, 0u);
    WriteBigEndian(bytes, 0x00, 0xdca24d00u, 4);
    WriteBigEndian(bytes, 0x08, bytes.size(), 8);
    WriteBigEndian(bytes, 0x10, count, 4);
    WriteBigEndian(bytes, 0x14, 0x40u, 4);
    for (std::size_t index = 0; index < count; ++index) {
        const auto entry = 0x60 + index * 0x40;
        std::memcpy(bytes.data() + entry, names[index], std::strlen(names[index]));
        WriteBigEndian(bytes, entry + 32, 0x60 + count * 0x40 + index * 16, 8);
        WriteBigEndian(bytes, entry + 40, 16u, 8);
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void CheckTrophyInfo() {
    constexpr int user = 0x10000000;
    constexpr std::uint32_t label = 7u;
    const std::filesystem::path package = "app0/sce_sys/trophy/trophy07.trp";
    const std::filesystem::path state = "_trophy/trophy07.268435456";
    std::filesystem::remove(state);
    WriteTrophyPackage(package);
    int context = -1;
    int handle = -1;
    Require(sceNpTrophyCreateContext(&context, user, label, 0u) == 0 && sceNpTrophyCreateHandle(&handle) == 0, "trophy context or handle not created");
    GameDetails game{};
    GameData gameData{};
    game.size = sizeof(game);
    gameData.size = sizeof(gameData);
    Require(sceNpTrophyGetGameInfo(context, handle, &game, &gameData) == TrophyNotRegistered, "game info answered before registration");
    Require(sceNpTrophyRegisterContext(context, handle, 0u) == 0, "trophy context not registered");
    Require(sceNpTrophyGetGameInfo(context, handle, nullptr, nullptr) == TrophyInvalidArgument, "game info without outputs accepted");
    game.size = sizeof(game) - 8u;
    Require(sceNpTrophyGetGameInfo(context, handle, &game, nullptr) == TrophyInvalidArgument, "game details of a wrong size accepted");
    game.size = sizeof(game);
    std::memset(game.title, 'x', sizeof(game.title));
    Require(sceNpTrophyGetGameInfo(context, handle, &game, &gameData) == 0, "game info refused");
    Require(game.numTrophies == 3u && game.numGroups == 0u && game.numPlatinum + game.numGold + game.numSilver + game.numBronze == 0u && game.title[0] == '\0', "game details are not the package's three trophies with unknown grades");
    Require(gameData.unlockedTrophies == 0u && gameData.progressPercentage == 0u, "game data reports progress before an unlock");
    int platinum = 0;
    Require(sceNpTrophyUnlockTrophy(context, handle, 1, &platinum) == 0, "trophy 1 not unlocked");
    Require(sceNpTrophyGetGameInfo(context, handle, nullptr, &gameData) == 0 && gameData.unlockedTrophies == 1u && gameData.progressPercentage == 33u, "game data does not count the unlock");
    TrophyDetails trophy{};
    TrophyData trophyData{};
    trophy.size = sizeof(trophy);
    trophyData.size = sizeof(trophyData);
    Require(sceNpTrophyGetTrophyInfo(context, handle, 1, &trophy, &trophyData) == 0, "trophy info refused");
    Require(trophy.trophyId == 1 && trophy.trophyGrade == 0 && trophy.groupId == -1 && !trophy.hidden && trophy.name[0] == '\0' && trophyData.trophyId == 1 && trophyData.unlocked, "trophy 1 info is wrong");
    Require(sceNpTrophyGetTrophyInfo(context, handle, 0, nullptr, &trophyData) == 0 && trophyData.trophyId == 0 && !trophyData.unlocked, "trophy 0 reported unlocked");
    Require(sceNpTrophyGetTrophyInfo(context, handle, 3, &trophy, &trophyData) == TrophyInvalidTrophyId, "a trophy past the package accepted");
    GroupDetails group{};
    GroupData groupData{};
    group.size = sizeof(group);
    groupData.size = sizeof(groupData);
    Require(sceNpTrophyGetGroupInfo(context, handle, -1, &group, &groupData) == 0, "base game group refused");
    Require(group.groupId == -1 && group.numTrophies == 3u && groupData.groupId == -1 && groupData.unlockedTrophies == 1u && groupData.progressPercentage == 33u, "base game group info is wrong");
    Require(sceNpTrophyGetGroupInfo(context, handle, 0, &group, &groupData) == TrophyInvalidGroupId, "a group the package does not have accepted");
    std::filesystem::remove(state);
    std::filesystem::remove(package);
    std::error_code ignored;
    for (auto directory = package.parent_path(); !directory.empty(); directory = directory.parent_path()) std::filesystem::remove(directory, ignored);
    std::filesystem::remove(state.parent_path(), ignored);
}

void CheckMatching2() {
    unsigned char param[0x30]{};
    const auto setSize = [&](std::uint64_t size) { std::memcpy(param + 0x20, &size, sizeof(size)); };
    Require(sceNpMatching2Terminate() == Matching2NotInitialized, "NpMatching2 terminated before initialization");
    Require(sceNpMatching2Initialize(nullptr) == Matching2InvalidArgument, "a null NpMatching2 parameter was accepted");
    setSize(0x20u);
    Require(sceNpMatching2Initialize(reinterpret_cast<const NpMatching2InitializeParameter*>(param)) == Matching2InvalidArgument, "an NpMatching2 parameter of size 0x20 was accepted");
    setSize(0x28u);
    Require(sceNpMatching2Initialize(reinterpret_cast<const NpMatching2InitializeParameter*>(param)) == 0, "an NpMatching2 parameter of size 0x28 was refused");
    Require(sceNpMatching2Initialize(reinterpret_cast<const NpMatching2InitializeParameter*>(param)) == Matching2AlreadyInitialized, "NpMatching2 initialized twice");
    Require(sceNpMatching2Terminate() == 0, "NpMatching2 did not terminate");
    setSize(0x30u);
    Require(sceNpMatching2Initialize(reinterpret_cast<const NpMatching2InitializeParameter*>(param)) == 0, "an NpMatching2 parameter with an SSL pool was refused");
    Require(sceNpMatching2Terminate() == 0, "NpMatching2 did not terminate after the SSL form");
}

}

int main() {
    CheckTrophyInfo();
    CheckMatching2();
    return 0;
}
