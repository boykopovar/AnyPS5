#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

static void Require(bool value) { if (!value) std::abort(); }
static std::map<std::string, std::uint32_t> ReadFileNumbers(void* directory) {
    std::map<std::string, std::uint32_t> numbers;
    rewinddir_nid_postfix(directory);

    while (auto* entry = readdir_nid_postfix(directory)) {
        Require(entry->fileNumber != 0);
        numbers[entry->name] = entry->fileNumber;
    }
    return numbers;
}
static std::uint32_t FileNumber(const std::filesystem::path& path) {
    FileStat status{};
    Require(lstat_nid_postfix(path.string().c_str(), &status) == 0);
    return static_cast<std::uint32_t>(status.st_ino);
}
static bool CreateLinks(const std::filesystem::path& root) {
#ifdef _WIN32
    if (CreateSymbolicLinkW((root / "file-link").c_str(), L"sample.txt", SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) == 0) {
        Require(GetLastError() == ERROR_PRIVILEGE_NOT_HELD);
        std::fprintf(stderr, "Windows readdir link checks skipped: creating symbolic links needs Developer Mode or administrator rights\n");
        return false;
    }
    Require(CreateSymbolicLinkW((root / "directory-link").c_str(), L"subdirectory",
        SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0);
#else
    std::filesystem::create_symlink("sample.txt", root / "file-link");
    std::filesystem::create_directory_symlink("subdirectory", root / "directory-link");
#endif
    return true;
}
int main() {
    const auto root = std::filesystem::path("anyps5-directory-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    Require(std::filesystem::create_directory(root / "subdirectory"));
    { std::ofstream file(root / "sample.txt"); file << "test"; }
    const bool links = CreateLinks(root);
    std::vector<const char*> numbered{"sample.txt", "subdirectory"};
    if (links) numbered.insert(numbered.end(), {"file-link", "directory-link"});
    void* directory = opendir_nid_postfix(root.string().c_str());
    Require(directory != nullptr);
    std::map<std::string, int> entries;
    std::map<std::string, std::uint32_t> fileNumbers;
    *__error_nid_postfix() = 13;
    while (auto* entry = readdir_nid_postfix(directory)) {
        Require(entry->nameLength == std::strlen(entry->name));
        Require(entry->recordLength == 8 + ((entry->nameLength + 4) & ~3));
        Require(entry->recordLength <= sizeof(*entry));
        entries[entry->name] = entry->type;
        Require(entry->fileNumber != 0);
        fileNumbers[entry->name] = entry->fileNumber;
    }
    Require(*__error_nid_postfix() == 13); // EOF preserves errno.
    for (const char* name : numbered) Require(fileNumbers.at(name) == FileNumber(root / name));
    if (links) Require(fileNumbers.at("file-link") != fileNumbers.at("sample.txt"));
    { std::ofstream file(root / "created.txt"); file << "new"; }
    auto changed = ReadFileNumbers(directory);
    Require(changed.at("created.txt") == FileNumber(root / "created.txt"));
    Require(changed.at("sample.txt") == fileNumbers.at("sample.txt"));
    Require(std::filesystem::remove(root / "created.txt"));
    changed = ReadFileNumbers(directory);
    Require(changed.count("created.txt") == 0 && changed == fileNumbers);
#ifdef _WIN32
    const HANDLE pending = CreateFileW((root / "pending.txt").c_str(), GENERIC_WRITE | DELETE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_NEW, 0, nullptr);
    Require(pending != INVALID_HANDLE_VALUE);
    FILE_DISPOSITION_INFO disposition{TRUE};
    Require(SetFileInformationByHandle(pending, FileDispositionInfo, &disposition, sizeof(disposition)) != 0);
    FileStat pendingStatus{};
    Require(lstat_nid_postfix((root / "pending.txt").string().c_str(), &pendingStatus) == -1);
    changed = ReadFileNumbers(directory);
    Require(changed.count("pending.txt") == 1 && changed.at("sample.txt") == fileNumbers.at("sample.txt"));
    Require(CloseHandle(pending) != 0);
    Require(ReadFileNumbers(directory) == fileNumbers);
#endif
    Require(entries.at("sample.txt") == 8);
    Require(entries.at("subdirectory") == 4);
    Require(entries.at(".") == 4 && entries.at("..") == 4);
    rewinddir_nid_postfix(directory);

    std::size_t count = 0;
    while (readdir_nid_postfix(directory)) ++count;
    Require(count == entries.size());
    rewinddir_nid_postfix(directory);

    std::map<std::string, int> copies;
    GuestDirectoryEntry buffer;
    GuestDirectoryEntry* result = nullptr;
    *__error_nid_postfix() = 13;
    for (;;) {
        std::memset(&buffer, 0x7f, sizeof(buffer));
        Require(readdir_r_nid_postfix(directory, &buffer, &result) == 0);
        if (!result) break;
        Require(result == &buffer);
        Require(buffer.nameLength == std::strlen(buffer.name));
        Require(buffer.recordLength == 8 + ((buffer.nameLength + 4) & ~3));
        for (std::size_t i = buffer.recordLength; i < sizeof(buffer); ++i)
            Require(reinterpret_cast<const unsigned char*>(&buffer)[i] == 0x7f);
        copies[buffer.name] = buffer.type;
    }
    Require(copies == entries);
    Require(*__error_nid_postfix() == 13);
    result = &buffer;
    Require(readdir_r_nid_postfix(directory, &buffer, &result) == 0 && result == nullptr);
    rewinddir_nid_postfix(directory);

    *__error_nid_postfix() = 13;
    auto* first = readdir_nid_postfix(directory);
    Require(first != nullptr);
    const std::string firstName = first->name;
    Require(readdir_r_nid_postfix(directory, &buffer, &result) == 0 && result == &buffer);
    Require(firstName != buffer.name && entries.count(buffer.name) == 1);
    result = &buffer;
    Require(readdir_r_nid_postfix(nullptr, &buffer, &result) == 9);
    Require(result == &buffer && *__error_nid_postfix() == 13);
    bool nullEntryThrows = false;
    try {
        readdir_r_nid_postfix(directory, nullptr, &result);

    } catch (const std::invalid_argument&) {
        nullEntryThrows = true;
    }
    Require(nullEntryThrows);
    bool nullResultThrows = false;
    try {
        readdir_r_nid_postfix(directory, &buffer, nullptr);

    } catch (const std::invalid_argument&) {
        nullResultThrows = true;
    }
    Require(nullResultThrows);
    rewinddir_nid_postfix(directory);

    Require(readdir_nid_postfix(directory) != nullptr);
    const std::int64_t second = telldir_nid_postfix(directory);
    Require(second != -1);
    auto* entryAfterTell = readdir_nid_postfix(directory);
    Require(entryAfterTell != nullptr);
    const std::string secondName = entryAfterTell->name;
    while (readdir_nid_postfix(directory)) {}
    seekdir_nid_postfix(directory, second);

    auto* entryAfterSeek = readdir_nid_postfix(directory);
    Require(entryAfterSeek != nullptr && secondName == entryAfterSeek->name);
    Require(entryAfterSeek->fileNumber == fileNumbers.at(secondName));
    *__error_nid_postfix() = 0;
    Require(telldir_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 9);
    *__error_nid_postfix() = 0;
    seekdir_nid_postfix(nullptr, second);

    Require(*__error_nid_postfix() == 9);
    Require(closedir_nid_postfix(directory) == 0);
    Require(opendir_nid_postfix((root / "missing").string().c_str()) == nullptr);
    Require(*__error_nid_postfix() == 2);
#ifndef _WIN32
    const auto symlinkLoop = root / "symlink-loop";
    std::filesystem::create_symlink("symlink-loop", symlinkLoop);
    Require(opendir_nid_postfix(symlinkLoop.string().c_str()) == nullptr);
    Require(*__error_nid_postfix() == 62);
    Require(std::filesystem::remove(symlinkLoop));
#endif
    Require(opendir_nid_postfix((root / "sample.txt").string().c_str()) == nullptr);
    Require(*__error_nid_postfix() == 20);
    Require(opendir_nid_postfix("") == nullptr && *__error_nid_postfix() == 2);
    Require(closedir_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 9);
    const std::u8string folder = u8"セーブ";
    const std::u8string file = u8"été.txt";
    Require(std::filesystem::create_directory(root / std::filesystem::path(folder)));
    { std::ofstream stream(root / std::filesystem::path(folder) / std::filesystem::path(file)); stream << "x"; }
    const std::string guestFolder = root.string() + "/" + std::string(folder.begin(), folder.end());
    void* unicode = opendir_nid_postfix(guestFolder.c_str());
    Require(unicode != nullptr);
    std::map<std::string, int> names;
    while (auto* entry = readdir_nid_postfix(unicode)) {
        Require(entry->nameLength == std::strlen(entry->name));
        names[entry->name] = entry->type;
    }
    Require(names.at(std::string(file.begin(), file.end())) == 8 && names.size() == 3);
    Require(closedir_nid_postfix(unicode) == 0);
    std::filesystem::remove_all(root / std::filesystem::path(folder));
    if (links) {
        std::filesystem::remove(root / "file-link");
        std::filesystem::remove(root / "directory-link");
    }
    std::filesystem::remove(root / "sample.txt");
    std::filesystem::remove(root / "subdirectory");
    std::filesystem::remove(root);
}
