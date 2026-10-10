#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "SceTypes.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t);
std::int64_t APS5_VABI sceKernelWrite(int, const void*, std::size_t);
int APS5_VABI sceKernelStat(const char*, FileStat*);
int APS5_VABI chdir_nid_postfix(const char*);
int APS5_VABI access_nid_postfix(const char*, int);
int APS5_VABI rename_nid_postfix(const char*, const char*);
int APS5_VABI remove_nid_postfix(const char*);
FileStream* APS5_VABI fopen_nid_postfix(const char*, const char*);
std::size_t APS5_VABI fread_nid_postfix(void*, std::size_t, std::size_t, FileStream*);
int APS5_VABI fclose_nid_postfix(FileStream*);
}

static void Require(bool value) { if (!value) std::abort(); }

static void CheckRead(const std::string& path, char expected) {
    const int fd = sceKernelOpen(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(fd >= 0);
    char value = 0;
    Require(sceKernelRead(fd, &value, 1) == 1 && value == expected);
    Require(sceKernelClose(fd) == 0);
}

int main() {
    const auto host = std::filesystem::canonical(std::filesystem::current_path());
    const auto name = "AnyPS5-Case-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = host / name;
    Require(std::filesystem::create_directories(directory / "Data" / "Nested"));
    { std::ofstream file(directory / "Data" / "Settings.ini"); file << 'x'; }
    auto lower = name;
    for (auto& c : lower) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    CheckRead("/" + lower + "/dAtA/SETTINGS.INI", 'x');
    FileStat stat{};
    Require(sceKernelStat(("/" + lower + "/DATA/settings.INI").c_str(), &stat) == 0);
    Require(chdir_nid_postfix(lower.c_str()) == 0);
    Require(access_nid_postfix("DATA/SETTINGS.ini", 4) == 0);
    auto* stream = fopen_nid_postfix("data\\SETTINGS.INI", "rb");
    char value = 0;
    Require(stream && fread_nid_postfix(&value, 1, 1, stream) == 1 && value == 'x');
    Require(fclose_nid_postfix(stream) == 0);
    Require(chdir_nid_postfix("DATA/../data") == 0);
    CheckRead("settings.INI", 'x');
    Require(chdir_nid_postfix("..") == 0);
    const int created = sceKernelOpen("data/nested/New.DAT", SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL | SCE_KERNEL_O_WRONLY, 0600);
    Require(created >= 0 && sceKernelWrite(created, "y", 1) == 1 && sceKernelClose(created) == 0);
    Require(std::filesystem::exists(directory / "Data" / "Nested" / "New.DAT"));
    CheckRead("DATA/NESTED/new.dat", 'y');
    Require(sceKernelOpen("data/nested/NEW.dat", SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL | SCE_KERNEL_O_WRONLY, 0600) == static_cast<int>(0x80020011u));
    Require(rename_nid_postfix("DATA/NESTED/new.dat", "data/nested/Moved.DAT") == 0);
    Require(remove_nid_postfix("DATA/NESTED/moved.dat") == 0);
    Require(sceKernelOpen("data/nested/missing", SCE_KERNEL_O_RDONLY, 0) == static_cast<int>(0x80020002u));
    AddPathAlias_nid_no_patch("case-mount", (directory / "Data").string().c_str());
    CheckRead("/CASE-MOUNT/settings.INI", 'x');
    Require(access_nid_postfix("/case-mount-other/Settings.ini", 0) == -1);
    BlockPathAlias_nid_no_patch("CASE-MOUNT");
    Require(access_nid_postfix("/case-mount/settings.ini", 0) == -1);
    AddPathAlias_nid_no_patch("Case-Mount", (directory / "Data").string().c_str());
    CheckRead("/case-MOUNT/settings.INI", 'x');
    RemovePathAlias_nid_no_patch("CASE-mount");
    Require(access_nid_postfix("/case-mount/settings.ini", 0) == -1);
#ifndef _WIN32
    std::filesystem::create_directory_symlink("Data", directory / "Linked");
    CheckRead("linked/SETTINGS.INI", 'x');
    { std::ofstream file(directory / "Data" / "SETTINGS.INI"); file << 'z'; }
    CheckRead("Data/Settings.ini", 'x');
    CheckRead("Data/SETTINGS.INI", 'z');
    bool ambiguous = false;
    try { CheckRead("data/settings.ini", 'x'); } catch (const std::runtime_error&) { ambiguous = true; }
    Require(ambiguous);
    std::filesystem::remove(directory / "Data" / "SETTINGS.INI");
    std::filesystem::remove(directory / "Data" / "Settings.ini");
    { std::ofstream file(directory / "Data" / "SETTINGS.ini"); file << 'n'; }
    CheckRead("data/settings.ini", 'n');
    std::filesystem::create_symlink("absent", directory / "Dangling");
    Require(sceKernelOpen("dangling", SCE_KERNEL_O_RDONLY, 0) == static_cast<int>(0x80020002u));
#endif
    Require(chdir_nid_postfix("/") == 0);
    Require(std::filesystem::current_path() == host);
    std::filesystem::remove_all(directory);
}
