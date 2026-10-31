#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/GuestDirectory.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace {

int s_fakeOffset = 0;

int APS5_VABI FakeClose(int) { return 0; }

int APS5_VABI FakeRead(int, char* buffer, int size) {
    if (s_fakeOffset > 0) return 0;
    GuestDirectoryEntry first{};
    first.fileNumber = 0;
    first.nameLength = 7;
    std::memcpy(first.name, "deleted", 8);
    first.recordLength = static_cast<std::uint16_t>(offsetof(GuestDirectoryEntry, name) + 8);
    first.type = 8;

    GuestDirectoryEntry second{};
    second.fileNumber = 42;
    second.nameLength = 6;
    std::memcpy(second.name, "active", 7);
    second.recordLength = static_cast<std::uint16_t>(offsetof(GuestDirectoryEntry, name) + 7);
    second.type = 8;

    const int total = first.recordLength + second.recordLength;
    if (size < total) return -1;
    std::memcpy(buffer, &first, first.recordLength);
    std::memcpy(buffer + first.recordLength, &second, second.recordLength);
    s_fakeOffset += total;
    return total;
}

std::int64_t APS5_VABI FakeSeek(int, std::int64_t, int) {
    s_fakeOffset = 0;
    return 0;
}

}

static void Require(bool value) { if (!value) std::abort(); }

static std::map<std::string, int> Entries(void* directory) {
    std::map<std::string, int> entries;
    while (auto* entry = readdir_nid_postfix(directory)) entries[entry->name] = entry->type;
    Require(closedir_nid_postfix(directory) == 0);
    return entries;
}

int main() {
    const auto root = std::filesystem::path("anyps5-opendir2-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    Require(std::filesystem::create_directory(root / "subdirectory"));
    { std::ofstream file(root / "sample.txt"); file << "test"; }
    const auto path = root.string();
    void* plain = opendir_nid_postfix(path.c_str());
    Require(plain != nullptr);
    const auto expected = Entries(plain);
    Require(expected.at("sample.txt") == 8 && expected.at("subdirectory") == 4);
    for (const int flags : {0x7, 0x6, 0x0, 0x1, 0x20, 0x27, 0x100, 0x40000000, static_cast<int>(0xffffffe7u)}) {
        void* directory = __opendir2_nid_postfix(path.c_str(), flags);
        Require(directory != nullptr);
        Require(Entries(directory) == expected);
    }
    for (const int flags : {0x8, 0x10, 0x18, 0xf, 0x1f, 0x7fffffff, -1}) {
        *__error_nid_postfix() = 1234;
        Require(__opendir2_nid_postfix(path.c_str(), flags) == nullptr && *__error_nid_postfix() == 1234);
        Require(__opendir2_nid_postfix(nullptr, flags) == nullptr && *__error_nid_postfix() == 1234);
    }
    const auto missing = (root / "missing").string();
    Require(opendir_nid_postfix(missing.c_str()) == nullptr);
    const int missingError = *__error_nid_postfix();
    for (const int flags : {0x7, 0x20}) {
        *__error_nid_postfix() = 0;
        Require(__opendir2_nid_postfix(missing.c_str(), flags) == nullptr && *__error_nid_postfix() == missingError);
    }
    *__error_nid_postfix() = 0;
    Require(_readdir_unlocked_nid_postfix(nullptr, 0) == nullptr && *__error_nid_postfix() == 9);
    *__error_nid_postfix() = 0;
    Require(_readdir_unlocked_nid_postfix(nullptr, 1) == nullptr && *__error_nid_postfix() == 9);
    *__error_nid_postfix() = 0;
    _seekdir_nid_postfix(nullptr, 0);
    Require(*__error_nid_postfix() == 9);
    void* unlockedDir = __opendir2_nid_postfix(path.c_str(), 0);
    Require(unlockedDir != nullptr);
    std::map<std::string, int> unlockedEntries;
    std::int64_t bookmark = -1;
    std::string bookmarkedName;
    while (auto* entry = _readdir_unlocked_nid_postfix(unlockedDir, 1)) {
        unlockedEntries[entry->name] = entry->type;
        if (bookmark < 0) {
            bookmark = telldir_nid_postfix(unlockedDir);
            Require(bookmark >= 0);
        } else if (bookmarkedName.empty()) {
            bookmarkedName = entry->name;
        }
    }
    Require(unlockedEntries == expected);
    Require(!bookmarkedName.empty());
    _seekdir_nid_postfix(unlockedDir, bookmark);
    auto* entryAfterSeek = _readdir_unlocked_nid_postfix(unlockedDir, 1);
    Require(entryAfterSeek != nullptr && bookmarkedName == entryAfterSeek->name);
    Require(closedir_nid_postfix(unlockedDir) == 0);

    s_fakeOffset = 0;
    void* fakeDir = OpenDescriptorDirectory_nid_no_patch("/virtual", 100, FakeClose, 0, FakeRead, FakeSeek);
    Require(fakeDir != nullptr);
    auto* skipped = _readdir_unlocked_nid_postfix(fakeDir, 1);
    Require(skipped != nullptr && skipped->fileNumber == 42 && std::strcmp(skipped->name, "active") == 0);
    _seekdir_nid_postfix(fakeDir, 0);
    auto* unskipped = _readdir_unlocked_nid_postfix(fakeDir, 0);
    Require(unskipped != nullptr && unskipped->fileNumber == 0 && std::strcmp(unskipped->name, "deleted") == 0);
    Require(closedir_nid_postfix(fakeDir) == 0);

    std::filesystem::remove(root / "sample.txt");
    std::filesystem::remove(root / "subdirectory");
    std::filesystem::remove(root);
}
