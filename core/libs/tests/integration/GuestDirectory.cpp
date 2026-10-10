#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <utility>

extern "C" {
void* APS5_VABI opendir_nid_postfix(const char*);
GuestDirectoryEntry* APS5_VABI readdir_nid_postfix(void*);
int APS5_VABI closedir_nid_postfix(void*);
void APS5_VABI rewinddir_nid_postfix(void*);
int* APS5_VABI __error_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int enoent = 2;
constexpr int ebadf = 9;
constexpr int eacces = 13;
constexpr int enotdir = 20;
constexpr int regularFile = 8;
constexpr int directoryType = 4;

const std::u8string unicodeFolder = u8"セーブ";
const std::u8string unicodeFile = u8"été.txt";

std::string Narrow(const std::u8string& text) {
    return std::string(text.begin(), text.end());
}

class DirectoryFixture {
public:
    DirectoryFixture()
        : name("anyps5-directory-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())),
          root(std::filesystem::current_path() / name) {
        Require(std::filesystem::create_directory(root), "create the test directory");
        Require(std::filesystem::create_directory(root / "subdirectory"), "create the subdirectory");
        std::ofstream(root / "sample.txt") << "test";
    }

    ~DirectoryFixture() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    DirectoryFixture(const DirectoryFixture&) = delete;
    DirectoryFixture& operator=(const DirectoryFixture&) = delete;

    std::string UnicodeFolder() const {
        Require(std::filesystem::create_directory(root / std::filesystem::path(unicodeFolder)), "create the unicode folder");
        std::ofstream(root / std::filesystem::path(unicodeFolder) / std::filesystem::path(unicodeFile)) << "x";
        return name + "/" + Narrow(unicodeFolder);
    }

    const std::string name;
    const std::filesystem::path root;
};

class OpenDirectory {
public:
    explicit OpenDirectory(const std::string& path) : handle(opendir_nid_postfix(path.c_str())) {
        Require(handle != nullptr, "opendir " + path);
    }

    ~OpenDirectory() {
        if (handle != nullptr) closedir_nid_postfix(handle);
    }

    OpenDirectory(const OpenDirectory&) = delete;
    OpenDirectory& operator=(const OpenDirectory&) = delete;

    void* Get() const { return handle; }

    int Close() {
        return closedir_nid_postfix(std::exchange(handle, nullptr));
    }

private:
    void* handle;
};

std::map<std::string, int> ReadTypes(void* directory) {
    std::map<std::string, int> entries;
    while (auto* entry = readdir_nid_postfix(directory)) entries[entry->name] = entry->type;
    return entries;
}

void RequireOpenFailure(const std::string& path, int expectedError) {
    Require(opendir_nid_postfix(path.c_str()) == nullptr, "opendir fails for " + path);
    RequireEqual(*__error_nid_postfix(), expectedError, "errno for " + path);
}

const Case recordLengths{"Readdir_EachEntry_ReportsConsistentNameAndRecordLengths", [] {
    const DirectoryFixture fixture;
    OpenDirectory directory(fixture.name);
    while (auto* entry = readdir_nid_postfix(directory.Get())) {
        const std::string name = entry->name;
        RequireEqual(static_cast<std::size_t>(entry->nameLength), std::strlen(entry->name), "name length of " + name);
        RequireEqual(static_cast<std::size_t>(entry->recordLength), static_cast<std::size_t>(8 + ((entry->nameLength + 4) & ~3)),
            "record length of " + name);
        Require(entry->recordLength <= sizeof(*entry), "record length fits the entry for " + name);
    }
}};

const Case endPreservesErrno{"Readdir_EndOfDirectory_PreservesErrno", [] {
    const DirectoryFixture fixture;
    OpenDirectory directory(fixture.name);
    *__error_nid_postfix() = eacces;
    ReadTypes(directory.Get());
    RequireEqual(*__error_nid_postfix(), eacces, "errno after the last entry");
}};

const Case entryTypes{"Readdir_FileAndDirectories_ReportsEntryTypes", [] {
    const DirectoryFixture fixture;
    OpenDirectory directory(fixture.name);
    const auto entries = ReadTypes(directory.Get());
    Require(entries.contains("sample.txt"), "sample.txt listed");
    Require(entries.contains("subdirectory"), "subdirectory listed");
    Require(entries.contains("."), ". listed");
    Require(entries.contains(".."), ".. listed");
    RequireEqual(entries.at("sample.txt"), regularFile, "sample.txt type");
    RequireEqual(entries.at("subdirectory"), directoryType, "subdirectory type");
    RequireEqual(entries.at("."), directoryType, ". type");
    RequireEqual(entries.at(".."), directoryType, ".. type");
}};

const Case rewindEntries{"Rewinddir_AfterReadingAllEntries_RestartsEnumeration", [] {
    const DirectoryFixture fixture;
    OpenDirectory directory(fixture.name);
    const auto entries = ReadTypes(directory.Get());
    rewinddir_nid_postfix(directory.Get());
    std::size_t count = 0;
    while (readdir_nid_postfix(directory.Get())) ++count;
    RequireEqual(count, entries.size(), "entries after rewind");
}};

const Case closeOpen{"Closedir_OpenDirectory_Succeeds", [] {
    const DirectoryFixture fixture;
    OpenDirectory directory(fixture.name);
    ReadTypes(directory.Get());
    RequireEqual(directory.Close(), 0, "closedir");
}};

const Case openMissing{"Opendir_MissingDirectory_FailsWithEnoent", [] {
    const DirectoryFixture fixture;
    RequireOpenFailure((std::filesystem::path(fixture.name) / "missing").string(), enoent);
}};

const Case openFile{"Opendir_RegularFile_FailsWithEnotdir", [] {
    const DirectoryFixture fixture;
    RequireOpenFailure((std::filesystem::path(fixture.name) / "sample.txt").string(), enotdir);
}};

const Case openEmpty{"Opendir_EmptyPath_FailsWithEnoent", [] {
    RequireOpenFailure("", enoent);
}};

const Case closeNull{"Closedir_NullHandle_FailsWithEbadf", [] {
    RequireEqual(closedir_nid_postfix(nullptr), -1, "closedir result");
    RequireEqual(*__error_nid_postfix(), ebadf, "errno");
}};

const Case unicodeNames{"Readdir_UnicodeDirectory_ReturnsUtf8Names", [] {
    const DirectoryFixture fixture;
    OpenDirectory directory(fixture.UnicodeFolder());
    std::map<std::string, int> names;
    while (auto* entry = readdir_nid_postfix(directory.Get())) {
        RequireEqual(static_cast<std::size_t>(entry->nameLength), std::strlen(entry->name), "name length");
        names[entry->name] = entry->type;
    }
    const std::string file = Narrow(unicodeFile);
    Require(names.contains(file), "unicode file listed");
    RequireEqual(names.at(file), regularFile, "unicode file type");
    RequireEqual(names.size(), std::size_t{3}, "entry count");
    RequireEqual(directory.Close(), 0, "closedir");
}};

} // namespace
