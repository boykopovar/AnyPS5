#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
extern "C" {
void* APS5_VABI fdopendir_nid_postfix(int);
GuestDirectoryEntry* APS5_VABI readdir_nid_postfix(void*);
int APS5_VABI closedir_nid_postfix(void*);
int APS5_VABI open_nid_postfix(const char*, int, int);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI fcntl_nid_postfix(int, int, ...);
std::int64_t APS5_VABI lseek_nid_postfix(int, std::int64_t, int);
int* APS5_VABI __error_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    const auto root = std::filesystem::path("anyps5-fdopendir-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    Require(std::filesystem::create_directory(root / "subdirectory"));
    { std::ofstream file(root / "sample.txt"); file << "test"; }
    const int fd = open_nid_postfix(root.string().c_str(), 0x20000, 0);
    Require(fd >= 0);
    Require(fcntl_nid_postfix(fd, 1) == 0);
    void* directory = fdopendir_nid_postfix(fd);
    Require(fcntl_nid_postfix(fd, 1) == 1);
    Require(directory != nullptr);
    std::map<std::string, int> entries;
    while (auto* entry = readdir_nid_postfix(directory)) {
        entries[entry->name] = entry->type;
    }
    Require(entries.at("sample.txt") == 8);
    Require(entries.at("subdirectory") == 4);
    Require(DescriptorIsOpen_nid_no_patch(fd));
    Require(closedir_nid_postfix(directory) == 0);
    Require(!DescriptorIsOpen_nid_no_patch(fd));
    Require(fdopendir_nid_postfix(fd) == nullptr && *__error_nid_postfix() == 9);
    Require(fdopendir_nid_postfix(-1) == nullptr && *__error_nid_postfix() == 9);
    const int file = open_nid_postfix((root / "sample.txt").string().c_str(), 0, 0);
    Require(file >= 0);
    Require(fdopendir_nid_postfix(file) == nullptr && *__error_nid_postfix() == 20);
    Require(fcntl_nid_postfix(file, 1) == 1);
    Require(DescriptorIsOpen_nid_no_patch(file));
    Require(close_nid_postfix(file) == 0);
#ifdef _WIN32
    const int advanced = open_nid_postfix(root.string().c_str(), 0x20000, 0);
    Require(advanced >= 0);
    Require(lseek_nid_postfix(advanced, 2, 0) == 2);
    void* rest = fdopendir_nid_postfix(advanced);
    Require(rest != nullptr);
    std::map<std::string, int> remaining;
    while (auto* entry = readdir_nid_postfix(rest)) remaining[entry->name] = entry->type;
    Require(remaining.size() == 2 && remaining.count("sample.txt") == 1 && remaining.count("subdirectory") == 1);
    Require(closedir_nid_postfix(rest) == 0);
#endif
    std::filesystem::remove(root / "sample.txt");
    std::filesystem::remove(root / "subdirectory");
    std::filesystem::remove(root);
}
