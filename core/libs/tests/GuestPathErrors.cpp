#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "Testing.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

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
    Require(::symlink("loop", "loop") == 0);
    Require(::symlink("second", "first") == 0 && ::symlink("first", "second") == 0);
    { std::ofstream file("plain.txt"); file << "plain"; }
    return root;
}

const std::string LongName(300, 'n');

void OpenAndUnlink() {
    Require(sceKernelOpen("loop", SCE_KERNEL_O_RDONLY, 0) == SceEloop);
    Require(sceKernelOpen("first", SCE_KERNEL_O_RDONLY, 0) == SceEloop);
    Require(sceKernelOpen("loop/inside", SCE_KERNEL_O_RDONLY, 0) == SceEloop);
    Require(sceKernelOpen(LongName.c_str(), SCE_KERNEL_O_RDONLY, 0) == SceEnametoolong);
    Require(sceKernelOpen((LongName + "/inside").c_str(), SCE_KERNEL_O_RDONLY, 0) == SceEnametoolong);
    Require(sceKernelOpen(LongName.c_str(), SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0644) == SceEnametoolong);
    Require(sceKernelOpen("missing-create/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0644) == SCE_KERNEL_ERROR_ENOENT);
    Require(PosixFails(open_nid_postfix("missing-create/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0644), 2));
    Require(sceKernelOpen("missing-create-exclusive/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0644) == SCE_KERNEL_ERROR_ENOENT);
    Require(PosixFails(open_nid_postfix("missing-create-exclusive/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0644), 2));
    Require(!std::filesystem::exists("missing-create") && !std::filesystem::exists("missing-create-exclusive"));
    Require(sceKernelOpen("loop/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0644) == SceEloop);
    Require(PosixFails(open_nid_postfix("loop/", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0644), GuestEloop));
    Require(PosixFails(open_nid_postfix("loop", SCE_KERNEL_O_RDONLY, 0), GuestEloop));
    Require(PosixFails(open_nid_postfix(LongName.c_str(), SCE_KERNEL_O_RDONLY, 0), GuestEnametoolong));
    Require(sceKernelUnlink(LongName.c_str()) == SceEnametoolong);
    Require(sceKernelUnlink("loop/inside") == SceEloop);
    Require(PosixFails(unlink_nid_postfix(LongName.c_str()), GuestEnametoolong));
    const int plain = sceKernelOpen("plain.txt", SCE_KERNEL_O_RDONLY, 0);
    Require(plain >= 0 && sceKernelClose(plain) == 0);
}

void Status() {
    FileStat status{};
    Require(sceKernelStat("loop", &status) == SceEloop);
    Require(sceKernelStat("first", &status) == SceEloop);
    Require(sceKernelStat("loop/inside", &status) == SceEloop);
    Require(sceKernelStat(LongName.c_str(), &status) == SceEnametoolong);
    Require(sceKernelStat((LongName + "/inside").c_str(), &status) == SceEnametoolong);
    Require(PosixFails(stat_nid_postfix("loop", &status), GuestEloop));
    Require(PosixFails(stat_nid_postfix(LongName.c_str(), &status), GuestEnametoolong));
    Require(PosixFails(lstat_nid_postfix(LongName.c_str(), &status), GuestEnametoolong));
    Require(lstat_nid_postfix("loop", &status) == 0 && (status.st_mode & 0170000) == 0120000);
    Require(sceKernelStat("plain.txt", &status) == 0 && status.st_size == 5);
    Require(sceKernelStat("missing.txt", &status) == SceKernelError(2));
}

void Directories() {
    Require(sceKernelMkdir("loop/inside", 0755) == SceEloop);
    Require(sceKernelMkdir(LongName.c_str(), 0755) == SceEnametoolong);
    Require(PosixFails(mkdir_nid_postfix("loop/inside", 0755), GuestEloop));
    Require(sceKernelRmdir("loop/inside") == SceEloop);
    Require(sceKernelRmdir(LongName.c_str()) == SceEnametoolong);
    Require(sceKernelRmdir((LongName + "/inside").c_str()) == SceEnametoolong);
    Require(PosixFails(rmdir_nid_postfix(LongName.c_str()), GuestEnametoolong));
    Require(sceKernelRmdir("loop") == SceKernelError(20));
    Require(sceKernelMkdir("made", 0755) == 0 && sceKernelRmdir("made") == 0);
    Require(sceKernelRmdir("made") == SceKernelError(2));
}

void Rename() {
    Require(sceKernelRename(LongName.c_str(), "target.txt") == SceEnametoolong);
    Require(sceKernelRename("loop/inside", "target.txt") == SceEloop);
    Require(sceKernelRename("plain.txt", LongName.c_str()) == SceEnametoolong);
    Require(sceKernelRename("plain.txt", "loop/inside") == SceEloop);
    Require(PosixFails(rename_nid_postfix(LongName.c_str(), "target.txt"), GuestEnametoolong));
    Require(sceKernelRename("missing.txt", "target.txt") == SceKernelError(2));
    Require(sceKernelRename("plain.txt", "moved.txt") == 0 && sceKernelRename("moved.txt", "plain.txt") == 0);
}

void Truncate() {
    Require(sceKernelTruncate_nid_postfix("loop", 0) == SceEloop);
    Require(sceKernelTruncate_nid_postfix(LongName.c_str(), 0) == SceEnametoolong);
    Require(PosixFails(truncate_nid_postfix("loop", 0), GuestEloop));
    Require(PosixFails(truncate_nid_postfix(LongName.c_str(), 0), GuestEnametoolong));
    Require(sceKernelTruncate_nid_postfix("missing.txt", 0) == SceKernelError(2));
    Require(sceKernelTruncate_nid_postfix("plain.txt", 2) == 0 && std::filesystem::file_size("plain.txt") == 2);
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
