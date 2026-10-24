#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

extern "C" {
int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode);
int APS5_VABI sceKernelUnlink(const char* path);
int APS5_VABI sceKernelClose(int d);
int APS5_VABI open_nid_postfix(const char* path, int flags, int mode);
int APS5_VABI unlink_nid_postfix(const char* path);
int APS5_VABI sceKernelStat(const char* path, FileStat* sb);
int APS5_VABI stat_nid_postfix(const char* path, FileStat* sb);
int APS5_VABI lstat_nid_postfix(const char* path, FileStat* sb);
int APS5_VABI sceKernelMkdir(const char* path, std::uint16_t mode);
int APS5_VABI sceKernelRmdir(const char* path);
int APS5_VABI mkdir_nid_postfix(const char* path, std::uint16_t mode);
int APS5_VABI rmdir_nid_postfix(const char* path);
int APS5_VABI sceKernelRename(const char* from, const char* to);
int APS5_VABI rename_nid_postfix(const char* from, const char* to);
int APS5_VABI sceKernelTruncate_nid_postfix(const char* path, std::int64_t length);
int APS5_VABI truncate_nid_postfix(const char* path, std::int64_t length);
int* APS5_VABI __error_nid_postfix();
}

namespace {

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "path error check failed at line %d\n", line);
        std::exit(1);
    }
}
#define Check(value) Require((value), __LINE__)

constexpr int GuestEloop = 62;
constexpr int GuestEnametoolong = 63;
constexpr int SceEloop = SceKernelError(GuestEloop);
constexpr int SceEnametoolong = SceKernelError(GuestEnametoolong);

bool PosixFails(int result, int error) {
    const bool failed = result == -1 && *__error_nid_postfix() == error;
    *__error_nid_postfix() = 0;
    return failed;
}

std::filesystem::path EnterFreshRoot() {
    const auto root = std::filesystem::temp_directory_path() /
        ("anyps5-path-errors-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    Check(::symlink("loop", "loop") == 0);
    Check(::symlink("second", "first") == 0 && ::symlink("first", "second") == 0);
    { std::ofstream file("plain.txt"); file << "plain"; }
    return root;
}

const std::string LongName(300, 'n');

void OpenAndUnlink() {
    Check(sceKernelOpen("loop", SCE_KERNEL_O_RDONLY, 0) == SceEloop);
    Check(sceKernelOpen("first", SCE_KERNEL_O_RDONLY, 0) == SceEloop);
    Check(sceKernelOpen("loop/inside", SCE_KERNEL_O_RDONLY, 0) == SceEloop);
    Check(sceKernelOpen(LongName.c_str(), SCE_KERNEL_O_RDONLY, 0) == SceEnametoolong);
    Check(sceKernelOpen((LongName + "/inside").c_str(), SCE_KERNEL_O_RDONLY, 0) == SceEnametoolong);
    Check(sceKernelOpen(LongName.c_str(), SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0644) == SceEnametoolong);
    Check(sceKernelOpen("missing-create/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0644) == SCE_KERNEL_ERROR_ENOENT);
    Check(PosixFails(open_nid_postfix("missing-create/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0644), 2));
    Check(sceKernelOpen("missing-create-exclusive/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0644) == SCE_KERNEL_ERROR_ENOENT);
    Check(PosixFails(open_nid_postfix("missing-create-exclusive/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0644), 2));
    Check(!std::filesystem::exists("missing-create") && !std::filesystem::exists("missing-create-exclusive"));
    Check(sceKernelOpen("loop/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0644) == SceEloop);
    Check(PosixFails(open_nid_postfix("loop/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0644), GuestEloop));
    Check(PosixFails(open_nid_postfix("loop", SCE_KERNEL_O_RDONLY, 0), GuestEloop));
    Check(PosixFails(open_nid_postfix(LongName.c_str(), SCE_KERNEL_O_RDONLY, 0), GuestEnametoolong));
    Check(sceKernelUnlink(LongName.c_str()) == SceEnametoolong);
    Check(sceKernelUnlink("loop/inside") == SceEloop);
    Check(PosixFails(unlink_nid_postfix(LongName.c_str()), GuestEnametoolong));
    const int plain = sceKernelOpen("plain.txt", SCE_KERNEL_O_RDONLY, 0);
    Check(plain >= 0 && sceKernelClose(plain) == 0);
}

void Status() {
    FileStat status{};
    Check(sceKernelStat("loop", &status) == SceEloop);
    Check(sceKernelStat("first", &status) == SceEloop);
    Check(sceKernelStat("loop/inside", &status) == SceEloop);
    Check(sceKernelStat(LongName.c_str(), &status) == SceEnametoolong);
    Check(sceKernelStat((LongName + "/inside").c_str(), &status) == SceEnametoolong);
    Check(PosixFails(stat_nid_postfix("loop", &status), GuestEloop));
    Check(PosixFails(stat_nid_postfix(LongName.c_str(), &status), GuestEnametoolong));
    Check(PosixFails(lstat_nid_postfix(LongName.c_str(), &status), GuestEnametoolong));
    Check(lstat_nid_postfix("loop", &status) == 0 && (status.st_mode & 0170000) == 0120000);
    Check(sceKernelStat("plain.txt", &status) == 0 && status.st_size == 5);
    Check(sceKernelStat("missing.txt", &status) == SceKernelError(2));
}

void Directories() {
    Check(sceKernelMkdir("loop/inside", 0755) == SceEloop);
    Check(sceKernelMkdir(LongName.c_str(), 0755) == SceEnametoolong);
    Check(PosixFails(mkdir_nid_postfix("loop/inside", 0755), GuestEloop));
    Check(sceKernelRmdir("loop/inside") == SceEloop);
    Check(sceKernelRmdir(LongName.c_str()) == SceEnametoolong);
    Check(sceKernelRmdir((LongName + "/inside").c_str()) == SceEnametoolong);
    Check(PosixFails(rmdir_nid_postfix(LongName.c_str()), GuestEnametoolong));
    Check(sceKernelRmdir("loop") == SceKernelError(20));
    Check(sceKernelMkdir("made", 0755) == 0 && sceKernelRmdir("made") == 0);
    Check(sceKernelRmdir("made") == SceKernelError(2));
}

void Rename() {
    Check(sceKernelRename(LongName.c_str(), "target.txt") == SceEnametoolong);
    Check(sceKernelRename("loop/inside", "target.txt") == SceEloop);
    Check(sceKernelRename("plain.txt", LongName.c_str()) == SceEnametoolong);
    Check(sceKernelRename("plain.txt", "loop/inside") == SceEloop);
    Check(PosixFails(rename_nid_postfix(LongName.c_str(), "target.txt"), GuestEnametoolong));
    Check(sceKernelRename("missing.txt", "target.txt") == SceKernelError(2));
    Check(sceKernelRename("plain.txt", "moved.txt") == 0 && sceKernelRename("moved.txt", "plain.txt") == 0);
}

void Truncate() {
    Check(sceKernelTruncate_nid_postfix("loop", 0) == SceEloop);
    Check(sceKernelTruncate_nid_postfix(LongName.c_str(), 0) == SceEnametoolong);
    Check(PosixFails(truncate_nid_postfix("loop", 0), GuestEloop));
    Check(PosixFails(truncate_nid_postfix(LongName.c_str(), 0), GuestEnametoolong));
    Check(sceKernelTruncate_nid_postfix("missing.txt", 0) == SceKernelError(2));
    Check(sceKernelTruncate_nid_postfix("plain.txt", 2) == 0 && std::filesystem::file_size("plain.txt") == 2);
}

}

int main() {
    const auto root = EnterFreshRoot();
    OpenAndUnlink();
    Status();
    Directories();
    Rename();
    Truncate();
    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
