#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#ifdef __linux__
#include <dirent.h>
#include <dlfcn.h>
static bool forceUnknownType = false;
static bool allTypesKnown = true;
extern "C" dirent* readdir(DIR* directory) {
    static const auto nativeRead = reinterpret_cast<dirent* (*)(DIR*)>(dlsym(RTLD_NEXT, "readdir"));
    if (!nativeRead) std::abort();
    auto* entry = nativeRead(directory);
    if (entry) {
        if (entry->d_type == DT_UNKNOWN) allTypesKnown = false;
        if (forceUnknownType) entry->d_type = DT_UNKNOWN;
    }
    return entry;
}
#endif
extern "C" {
void* APS5_VABI opendir_nid_postfix(const char*);
GuestDirectoryEntry* APS5_VABI readdir_nid_postfix(void*);
int APS5_VABI closedir_nid_postfix(void*);
void APS5_VABI rewinddir_nid_postfix(void*);
int* APS5_VABI __error_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    const auto root = std::filesystem::path("anyps5-directory-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    Require(std::filesystem::create_directory(root / "subdirectory"));
    { std::ofstream file(root / "sample.txt"); file << "test"; }
#ifdef __linux__
    std::filesystem::create_symlink("sample.txt", root / "link");
    std::filesystem::create_symlink("missing", root / "dangling-link");
#endif
    void* directory = opendir_nid_postfix(root.string().c_str());
    Require(directory != nullptr);
    std::map<std::string, int> entries;
    *__error_nid_postfix() = 13;
    while (auto* entry = readdir_nid_postfix(directory)) {
        Require(entry->nameLength == std::strlen(entry->name));
        Require(entry->recordLength == 8 + ((entry->nameLength + 4) & ~3));
        Require(entry->recordLength <= sizeof(*entry));
        entries[entry->name] = entry->type;
    }
    Require(*__error_nid_postfix() == 13); // EOF preserves errno.
    Require(entries.at("sample.txt") == 8);
    Require(entries.at("subdirectory") == 4);
    Require(entries.at(".") == 4 && entries.at("..") == 4);
#ifdef __linux__
    Require(entries.at("link") == 10 && entries.at("dangling-link") == 10);
    if (allTypesKnown) {
        const auto renamed = root.string() + "-renamed";
        std::filesystem::rename(root, renamed);
        rewinddir_nid_postfix(directory);
        std::map<std::string, int> renamedEntries;
        while (auto* entry = readdir_nid_postfix(directory)) renamedEntries[entry->name] = entry->type;
        std::filesystem::rename(renamed, root);
        Require(renamedEntries == entries);
    }
    forceUnknownType = true;
    rewinddir_nid_postfix(directory);
    std::map<std::string, int> unknownEntries;
    while (auto* entry = readdir_nid_postfix(directory)) unknownEntries[entry->name] = entry->type;
    Require(unknownEntries == entries);
    Require(*__error_nid_postfix() == 13);
    forceUnknownType = false;
#endif
    rewinddir_nid_postfix(directory);
    std::size_t count = 0;
    while (readdir_nid_postfix(directory)) ++count;
    Require(count == entries.size());
    Require(closedir_nid_postfix(directory) == 0);
    Require(opendir_nid_postfix((root / "missing").string().c_str()) == nullptr);
    Require(*__error_nid_postfix() == 2);
    Require(opendir_nid_postfix((root / "sample.txt").string().c_str()) == nullptr);
    Require(*__error_nid_postfix() == 20);
    Require(opendir_nid_postfix("") == nullptr && *__error_nid_postfix() == 2);
    Require(closedir_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 9);
    std::filesystem::remove_all(root);
}
