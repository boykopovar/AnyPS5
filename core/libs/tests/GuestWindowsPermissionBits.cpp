#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <windows.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

constexpr std::uint16_t DirectoryType = 0040000;
constexpr std::uint16_t RegularType = 0100000;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "WINDOWS PERMISSION BITS: %s (errno %d)\n", message, *__error_nid_postfix());
        std::abort();
    }
}

std::uint16_t PathMode(const std::string& path) {
    FileStat status{};
    Require(stat_nid_postfix(path.c_str(), &status) == 0, "stat failed");
    FileStat link{};
    Require(lstat_nid_postfix(path.c_str(), &link) == 0, "lstat failed");
    Require(link.st_mode == status.st_mode, "lstat and stat modes differ");
    return status.st_mode;
}

std::uint16_t DescriptorMode(int descriptor) {
    FileStat status{};
    Require(fstat_nid_disambig1_nid_postfix(descriptor, &status) == 0, "fstat failed");
    FileStat kernel{};
    Require(sceKernelFstat(descriptor, &kernel) == 0, "sceKernelFstat failed");
    Require(kernel.st_mode == status.st_mode, "fstat and sceKernelFstat modes differ");
    return status.st_mode;
}

int Create(const std::string& path, int flags, int mode) {
    const int descriptor = open_nid_postfix(path.c_str(), flags, mode);
    Require(descriptor >= 0, "open failed");
    return descriptor;
}

void AuditDirectories(const std::string& root) {
    const std::string privateDirectory = root + "/private";
    Require(mkdir_nid_postfix(privateDirectory.c_str(), 0700) == 0, "mkdir 0700 failed");
    Require(PathMode(privateDirectory) == (DirectoryType | 0700), "mkdir 0700 must read back 040700");

    const std::string masked = root + "/masked";
    Require(mkdir_nid_postfix(masked.c_str(), 01777) == 0, "mkdir 01777 failed");
    Require(PathMode(masked) == (DirectoryType | 0755), "mkdir keeps only the access bits less the umask");

    const std::string kernel = root + "/kernel";
    Require(sceKernelMkdir(kernel.c_str(), 0750) == 0, "sceKernelMkdir failed");
    Require(PathMode(kernel) == (DirectoryType | 0750), "sceKernelMkdir 0750 must read back 040750");

    const int directory = open_nid_postfix(privateDirectory.c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Require(directory >= 0, "open directory failed");
    Require(DescriptorMode(directory) == (DirectoryType | 0700), "directory descriptor must report 040700");
    Require(mkdirat_nid_postfix(directory, "nested", 0710) == 0, "mkdirat failed");
    Require(fchmod_nid_postfix(directory, 0750) == 0, "fchmod directory failed");
    Require(DescriptorMode(directory) == (DirectoryType | 0750), "fchmod directory 0750 must read back 040750");
    Require(close_nid_postfix(directory) == 0, "close directory failed");
    Require(PathMode(privateDirectory + "/nested") == (DirectoryType | 0710), "mkdirat 0710 must read back 040710");

    Require(chmod_nid_postfix(privateDirectory.c_str(), 0711) == 0, "chmod directory failed");
    Require(PathMode(privateDirectory) == (DirectoryType | 0711), "chmod directory 0711 must read back 040711");
}

void AuditFiles(const std::string& root) {
    const std::string secret = root + "/secret";
    int descriptor = Create(secret, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0600);
    Require(DescriptorMode(descriptor) == (RegularType | 0600), "write-only descriptor must report 0100600");
    Require(close_nid_postfix(descriptor) == 0, "close failed");
    Require(PathMode(secret) == (RegularType | 0600), "O_CREAT 0600 must read back 0100600");

    descriptor = Create(secret, SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT, 0666);
    Require(close_nid_postfix(descriptor) == 0, "close failed");
    descriptor = Create(secret, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC, 0666);
    Require(close_nid_postfix(descriptor) == 0, "close failed");
    Require(PathMode(secret) == (RegularType | 0600), "opening an existing file with O_CREAT keeps its mode");

    const std::string truncated = root + "/truncated";
    SetLastError(ERROR_ALREADY_EXISTS);
    descriptor = Create(truncated, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC, 0640);
    Require(DescriptorMode(descriptor) == (RegularType | 0640), "O_CREAT | O_TRUNC on an absent path must store the requested mode");
    Require(close_nid_postfix(descriptor) == 0, "close failed");
    descriptor = Create(truncated, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC, 0444);
    Require(close_nid_postfix(descriptor) == 0, "close failed");
    Require(PathMode(truncated) == (RegularType | 0640), "O_CREAT | O_TRUNC on an existing file keeps its mode");
    Require((GetFileAttributesA(truncated.c_str()) & FILE_ATTRIBUTE_READONLY) == 0, "O_CREAT | O_TRUNC must not apply the mode to an existing file");

    const std::string directory = root + "/directory";
    Require(mkdir_nid_postfix(directory.c_str(), 0755) == 0, "mkdir failed");
    Require(open_nid_postfix(directory.c_str(), SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0666) < 0, "O_CREAT on a directory must fail");
    Require(*__error_nid_postfix() != 17, "O_CREAT without O_EXCL on a directory must not fail with EEXIST");

    const std::string special = root + "/special";
    descriptor = Create(special, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 07777);
    Require(close_nid_postfix(descriptor) == 0, "close failed");
    Require(PathMode(special) == (RegularType | 06755), "O_CREAT drops the sticky bit and the umask bits");

    const std::string readOnly = root + "/read-only";
    descriptor = Create(readOnly, SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_CREAT, 0444);
    Require(DescriptorMode(descriptor) == (RegularType | 0444), "read-only descriptor must report 0100444");
    Require(close_nid_postfix(descriptor) == 0, "close failed");

    const std::uint16_t previous = umask_nid_postfix(077);
    const std::string masked = root + "/masked-file";
    descriptor = Create(masked, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT, 0666);
    Require(close_nid_postfix(descriptor) == 0, "close failed");
    Require(umask_nid_postfix(previous) == 077, "umask must return the mask it replaces");
    Require(PathMode(masked) == (RegularType | 0600), "O_CREAT applies the process umask");

    FileStat before{};
    Require(stat_nid_postfix(secret.c_str(), &before) == 0, "stat before chmod failed");
    Require(chmod_nid_postfix(secret.c_str(), 04640) == 0, "chmod failed");
    FileStat after{};
    Require(stat_nid_postfix(secret.c_str(), &after) == 0, "stat after chmod failed");
    Require(after.st_mode == (RegularType | 04640), "chmod 04640 must read back 0104640");
    Require(after.st_mtim.tv_sec == before.st_mtim.tv_sec && after.st_mtim.tv_nsec == before.st_mtim.tv_nsec, "chmod must not change st_mtim");

    descriptor = Create(secret, SCE_KERNEL_O_RDONLY, 0);
    Require(fchmod_nid_postfix(descriptor, 0604) == 0, "fchmod failed");
    Require(DescriptorMode(descriptor) == (RegularType | 0604), "fchmod 0604 must read back on the descriptor");
    Require(fchmod_nid_postfix(descriptor, 0400) == 0, "fchmod 0400 through a read-only descriptor failed");
    Require(DescriptorMode(descriptor) == (RegularType | 0400), "fchmod 0400 must read back on a read-only descriptor");
    Require((GetFileAttributesA(secret.c_str()) & FILE_ATTRIBUTE_READONLY) != 0, "fchmod 0400 must set the read-only attribute");
    Require(fchmod_nid_postfix(descriptor, 0604) == 0, "fchmod back to 0604 through a read-only descriptor failed");
    Require((GetFileAttributesA(secret.c_str()) & FILE_ATTRIBUTE_READONLY) == 0, "fchmod 0604 must clear the read-only attribute");
    Require(close_nid_postfix(descriptor) == 0, "close failed");

    const std::string moved = root + "/moved";
    const std::string replacement = root + "/replacement";
    descriptor = Create(moved, SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0644);
    Require(rename_nid_postfix(moved.c_str(), replacement.c_str()) == 0, "rename open file failed");
    const int other = Create(moved, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0640);
    Require(close_nid_postfix(other) == 0, "close failed");
    Require(fchmod_nid_postfix(descriptor, 0400) == 0, "fchmod after rename failed");
    Require(DescriptorMode(descriptor) == (RegularType | 0400), "fchmod after rename must change the open file");
    Require(close_nid_postfix(descriptor) == 0, "close failed");
    Require(PathMode(replacement) == (RegularType | 0400), "fchmod must follow the open file to its new name");
    Require((GetFileAttributesA(replacement.c_str()) & FILE_ATTRIBUTE_READONLY) != 0, "fchmod 0400 must set the read-only attribute on the open file");
    Require(PathMode(moved) == (RegularType | 0640), "fchmod must not change the file now at the old name");
    Require((GetFileAttributesA(moved.c_str()) & FILE_ATTRIBUTE_READONLY) == 0, "fchmod must not change the attributes at the old name");
    Require(chmod_nid_postfix(replacement.c_str(), 0600) == 0, "chmod renamed file back failed");

    const std::string unlinked = root + "/unlinked";
    descriptor = Create(unlinked, SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0644);
    Require(unlink_nid_postfix(unlinked.c_str()) == 0, "unlink open file failed");
    Require(fchmod_nid_postfix(descriptor, 0600) == 0, "fchmod on an unlinked open file failed");
    Require(DescriptorMode(descriptor) == (RegularType | 0600), "fchmod on an unlinked open file must change its mode");
    Require(close_nid_postfix(descriptor) == 0, "close failed");

    Require(sceKernelChmod_nid_postfix(secret.c_str(), 0400) == 0, "sceKernelChmod failed");
    Require(PathMode(secret) == (RegularType | 0400), "sceKernelChmod 0400 must read back 0100400");
    Require(chmod_nid_postfix(secret.c_str(), 0600) == 0, "chmod back to 0600 failed");
    Require(chmod_nid_postfix(readOnly.c_str(), 0600) == 0, "chmod read-only file failed");
}

void AuditHostFiles(const std::string& root) {
    const std::string host = root + "/host";
    {
        std::ofstream output(host, std::ios::binary);
        Require(output.is_open(), "create host file failed");
        output << "abc";
    }
    Require(PathMode(host) == (RegularType | 0666), "a file without stored bits keeps the attribute mode");
    Require(SetFileAttributesA(host.c_str(), FILE_ATTRIBUTE_READONLY) != 0, "set read-only attribute failed");
    Require(PathMode(host) == (RegularType | 0444), "a read-only file without stored bits reports 0100444");
    Require(SetFileAttributesA(host.c_str(), FILE_ATTRIBUTE_NORMAL) != 0, "clear read-only attribute failed");
}

}

int main() {
    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::string root = "anyps5-permission-bits-" + suffix;
    std::filesystem::remove_all(root);
    Require(umask_nid_postfix(022) == 022, "the initial umask must be 022");
    Require(mkdir_nid_postfix(root.c_str(), 0755) == 0, "mkdir root failed");
    AuditDirectories(root);
    AuditFiles(root);
    AuditHostFiles(root);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::puts("GuestWindowsPermissionBits tests passed");
    return 0;
}
