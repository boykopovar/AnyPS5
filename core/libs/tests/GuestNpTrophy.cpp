#include "prx/libSceNpTrophy/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80551604u);
constexpr int InsufficientBuffer = static_cast<int>(0x80551605u);
constexpr int Abort = static_cast<int>(0x80551607u);
constexpr int InvalidHandle = static_cast<int>(0x80551608u);
constexpr int InvalidContext = static_cast<int>(0x80551609u);
constexpr int InvalidTrophyId = static_cast<int>(0x8055160au);
constexpr int AlreadyUnlocked = static_cast<int>(0x8055160cu);
constexpr int NotRegistered = static_cast<int>(0x8055160fu);
constexpr int AlreadyRegistered = static_cast<int>(0x80551610u);
constexpr int ContextAlreadyExists = static_cast<int>(0x80551613u);
constexpr int TitleConfNotInstalled = static_cast<int>(0x80551626u);
constexpr int TrophyNotUnlocked = static_cast<int>(0x8055161au);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpTrophy: %s\n", message);
        std::abort();
    }
}

void PutBigEndian(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint64_t value, int width) {
    for (int i = 0; i < width; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * (width - 1 - i)));
}

void WritePackage(const std::filesystem::path& path, const std::vector<std::pair<std::string, std::string>>& files) {
    const std::size_t header = 0x60, entry = 0x40;
    std::vector<std::uint8_t> bytes(header + entry * files.size());
    PutBigEndian(bytes, 0, 0xdca24d00u, 4);
    PutBigEndian(bytes, 0x10, files.size(), 4);
    PutBigEndian(bytes, 0x14, entry, 4);
    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto at = header + entry * i;
        std::memcpy(bytes.data() + at, files[i].first.data(), files[i].first.size());
        PutBigEndian(bytes, at + 32, bytes.size(), 8);
        PutBigEndian(bytes, at + 40, files[i].second.size(), 8);
        bytes.insert(bytes.end(), files[i].second.begin(), files[i].second.end());
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-np-trophy-" + std::to_string(std::random_device{}()));
    std::filesystem::remove_all(root);
    WritePackage(root / "app0/sce_sys/trophy/trophy00.trp", {{"TROPCONF.ESFM", "conf"}, {"ICON0.PNG", "game-icon"}, {"TROP000.PNG", "first"}, {"TROP001.PNG", "second"}});
    std::filesystem::current_path(root);

    int context = 0, handle = 0, missing = 0, platinum = 0;
    Require(sceNpTrophyCreateContext(nullptr, 1, 0, 0) == InvalidArgument, "null context");
    Require(sceNpTrophyCreateContext(&context, 1, 0, 1) == InvalidArgument, "context options");
    Require(sceNpTrophyCreateContext(&context, 1, 0, 0) == 0, "create the context");
    Require(sceNpTrophyCreateContext(&missing, 1, 0, 0) == ContextAlreadyExists, "second context for the same user and label");
    Require(sceNpTrophyCreateHandle(&handle) == 0, "create the handle");

    std::uint32_t flags[4] = {}, count = 0;
    Require(sceNpTrophyGetTrophyUnlockState(context, handle, reinterpret_cast<TrophyFlagArray*>(flags), &count) == NotRegistered, "state before registration");
    Require(sceNpTrophyRegisterContext(context, handle, 0) == 0, "register the context");
    Require(sceNpTrophyRegisterContext(context, handle, 0) == AlreadyRegistered, "register twice");
    Require(sceNpTrophyGetTrophyUnlockState(context, handle, reinterpret_cast<TrophyFlagArray*>(flags), &count) == 0 && count == 2 && flags[0] == 0, "two trophies, none unlocked");

    std::uint64_t size = 0;
    Require(sceNpTrophyGetGameIcon(context, handle, nullptr, &size) == 0 && size == 9, "game icon size");
    char icon[16] = {};
    size = 4;
    Require(sceNpTrophyGetGameIcon(context, handle, icon, &size) == InsufficientBuffer, "game icon into a short buffer");
    size = sizeof(icon);
    Require(sceNpTrophyGetGameIcon(context, handle, icon, &size) == 0 && size == 9 && std::memcmp(icon, "game-icon", 9) == 0, "game icon bytes");

    size = sizeof(icon);
    Require(sceNpTrophyGetTrophyIcon(context, handle, 1, icon, &size) == TrophyNotUnlocked, "icon of a locked trophy");
    Require(sceNpTrophyUnlockTrophy(context, handle, 2, &platinum) == InvalidTrophyId, "unlock a trophy past the count");
    Require(sceNpTrophyUnlockTrophy(context, handle, 1, &platinum) == 0 && platinum == -1, "unlock trophy 1");
    Require(sceNpTrophyUnlockTrophy(context, handle, 1, &platinum) == AlreadyUnlocked, "unlock trophy 1 twice");
    Require(sceNpTrophyGetTrophyUnlockState(context, handle, reinterpret_cast<TrophyFlagArray*>(flags), &count) == 0 && flags[0] == 2u, "trophy 1 unlocked");
    size = sizeof(icon);
    Require(sceNpTrophyGetTrophyIcon(context, handle, 1, icon, &size) == 0 && size == 6 && std::memcmp(icon, "second", 6) == 0, "icon of the unlocked trophy");
    int saved = -1;
    {
        std::ifstream state(root / "_trophy/trophy00.1");
        state >> saved;
    }
    Require(saved == 1, "unlock state saved for the user");

    std::filesystem::create_directories(root / "_trophy/trophy00.1.tmp");
    bool saveFailed = false;
    try {
        sceNpTrophyUnlockTrophy(context, handle, 0, &platinum);
    } catch (const std::exception&) {
        saveFailed = true;
    }
    std::filesystem::remove_all(root / "_trophy/trophy00.1.tmp");
    Require(saveFailed, "an unlock whose state cannot be written reports the failure");
    Require(sceNpTrophyGetTrophyUnlockState(context, handle, reinterpret_cast<TrophyFlagArray*>(flags), &count) == 0 && flags[0] == 2u, "a failed save leaves the trophy locked");
    Require(sceNpTrophyUnlockTrophy(context, handle, 0, &platinum) == 0, "the unlock succeeds once the state can be written");

    Require(sceNpTrophyCreateContext(&missing, 1, 1, 0) == 0, "context for a label without a package");
    Require(sceNpTrophyRegisterContext(missing, handle, 0) == TitleConfNotInstalled, "register without a trophy package");
    Require(sceNpTrophyCreateHandle(nullptr) == InvalidArgument, "null handle creation");
    Require(sceNpTrophyCreateContext(nullptr, 1, 0, 0) == InvalidArgument, "null context creation");
    Require(sceNpTrophyGetGameIcon(context, handle, icon, nullptr) == InvalidArgument, "null game icon size pointer");
    Require(sceNpTrophyGetGroupIcon(context, handle, 0, icon, nullptr) == InvalidArgument, "null group icon size pointer");
    Require(sceNpTrophyGetTrophyIcon(context, handle, 0, icon, nullptr) == InvalidArgument, "null trophy icon size pointer");
    Require(sceNpTrophyDestroyHandle(-1) == InvalidHandle, "invalid handle destruction");
    Require(sceNpTrophyDestroyContext(missing) == 0, "destroy the second context");
    int live = 0;
    Require(sceNpTrophyCreateHandle(&live) == 0, "second handle");
    Require(sceNpTrophyAbortHandle(handle) == 0, "abort a live handle");
    size = sizeof(icon);
    Require(sceNpTrophyGetTrophyUnlockState(context, handle, reinterpret_cast<TrophyFlagArray*>(flags), &count) == Abort, "unlock state through an aborted handle");
    Require(sceNpTrophyGetGameIcon(context, handle, icon, &size) == Abort, "game icon through an aborted handle");
    Require(sceNpTrophyUnlockTrophy(context, handle, 0, &platinum) == Abort, "unlock through an aborted handle");
    Require(sceNpTrophyRegisterContext(context, handle, 0) == Abort, "register through an aborted handle");
    Require(sceNpTrophyAbortHandle(handle) == 0, "abort the handle again");
    Require(sceNpTrophyGetTrophyUnlockState(context, live, reinterpret_cast<TrophyFlagArray*>(flags), &count) == 0, "the other handle still works");
    Require(sceNpTrophyDestroyHandle(live) == 0, "destroy the second handle");
    Require(sceNpTrophyAbortHandle(999) == InvalidHandle && sceNpTrophyAbortHandle(-1) == InvalidHandle, "abort an unknown handle");
    Require(sceNpTrophyDestroyHandle(handle) == 0 && sceNpTrophyDestroyContext(context) == 0, "destroy the handle and context");
    Require(sceNpTrophyAbortHandle(handle) == InvalidHandle, "abort a destroyed handle");
    Require(sceNpTrophyDestroyContext(context) == InvalidContext, "destroy the context twice");

    std::filesystem::current_path(root.parent_path());
    std::error_code error;
    std::filesystem::remove_all(root, error);
    return 0;
}
