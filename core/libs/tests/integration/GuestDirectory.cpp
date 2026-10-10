#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
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
    std::filesystem::remove(root / "sample.txt");
    std::filesystem::remove(root / "subdirectory");
    std::filesystem::remove(root);
}
