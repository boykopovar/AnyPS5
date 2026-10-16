#include "AppArchiveFixture.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

struct KernelIovec {
    void* base;
    std::size_t length;
};

extern "C" {
int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode);
int APS5_VABI sceKernelClose(int d);
std::int64_t APS5_VABI sceKernelRead(int d, void* buf, std::size_t nbytes);
std::int64_t APS5_VABI sceKernelWrite(int d, const void* buf, std::size_t nbytes);
std::int64_t APS5_VABI sceKernelPread(int d, void* buf, std::size_t nbytes, std::int64_t offset);
std::int64_t APS5_VABI sceKernelReadv(int d, const KernelIovec* iov, int iovcnt);
std::int64_t APS5_VABI sceKernelPreadv(int d, const KernelIovec* iov, int iovcnt, std::int64_t offset);
std::int64_t APS5_VABI sceKernelLseek(int d, std::int64_t offset, int whence);
int APS5_VABI sceKernelStat(const char* path, FileStat* sb);
int APS5_VABI sceKernelFstat(int d, FileStat* sb);
int APS5_VABI sceKernelCheckReachability(const char* path);
int APS5_VABI dup_nid_postfix(int d);
int APS5_VABI dup2_nid_postfix(int from, int to);
int APS5_VABI close_nid_postfix(int d);
std::int64_t APS5_VABI pread_nid_postfix(int d, void* buf, std::size_t nbytes, std::int64_t offset);
std::int64_t APS5_VABI lseek_nid_postfix(int d, std::int64_t offset, int whence);
int APS5_VABI fcntl_nid_postfix(int d, int command, ...);
int APS5_VABI ioctl_nid_postfix(int d, std::uint64_t request, void* argument);
void* APS5_VABI opendir_nid_postfix(const char* path);
void* APS5_VABI fdopen_nid_postfix(int d, const char* mode);
int APS5_VABI sceKernelAioSubmitReadCommands(KernelAioRwRequest* req, std::int32_t size, std::int32_t prio, std::int32_t* id);
int APS5_VABI sceKernelAioWaitRequest(std::int32_t id, std::int32_t* state, std::uint32_t* usec);
int APS5_VABI sceKernelAioDeleteRequest(std::int32_t id, std::int32_t* ret);
int APS5_VABI flock_nid_postfix(int d, int operation);
int APS5_VABI _fstatfs_nid_postfix(int d, void* buf);
int APS5_VABI ftruncate_nid_postfix(int d, std::int64_t length);
int APS5_VABI sceKernelFsync(int d);
int APS5_VABI sceKernelFdatasync(int d);
int APS5_VABI sceKernelGetdents(int d, char* buf, int nbytes);
int APS5_VABI sceKernelFchmod(int d, std::uint16_t mode);
int APS5_VABI sceKernelFutimes(int d, const void* times);
void* APS5_VABI mmap_nid_postfix(void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset) noexcept;
void* APS5_VABI __cxa_current_primary_exception_nid_postfix();
std::type_info* APS5_VABI __cxa_current_exception_type_nid_postfix();
using TerminateHandler = void (*)();
TerminateHandler APS5_VABI _ZSt13set_terminatePFvvE_nid_postfix(TerminateHandler);
}

namespace {

void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "app0.zar kernel check failed at line %d\n", line);
        std::exit(1);
    }
}
#define Check(value) Require((value), __LINE__)

void OnMmapRefused() {
    const auto* type = __cxa_current_exception_type_nid_postfix();
    const auto* error = static_cast<const std::runtime_error*>(__cxa_current_primary_exception_nid_postfix());
    const bool refused = type != nullptr && error != nullptr && std::strcmp(type->name(), typeid(std::runtime_error).name()) == 0 &&
        std::strstr(error->what(), "mmap on a file read from app0.zar") != nullptr && std::strstr(error->what(), "not implemented") != nullptr;
    std::_Exit(refused ? 0 : 1);
}

template <typename TCall> bool NotImplemented(TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::strstr(error.what(), "not implemented") != nullptr;
    }
    return false;
}

constexpr std::size_t BigSize = 3 * 65536 + 77;
constexpr std::uint16_t TypeMask = 0170000;
constexpr std::uint16_t RegularType = 0100000;
constexpr std::uint16_t DirectoryType = 0040000;
constexpr int DuplicateFrom = 0;
constexpr std::uint64_t ReadableRequest = 0x4004667f;

void Archive(const std::filesystem::path& root) {
    AppArchiveFixture::WriteArchive(root / "app0.zar", {
        {"Data/Big.bin", AppArchiveFixture::Pattern(BigSize)},
        {"Data/Small.txt", {'z', 'a', 'r'}},
        {"sce_sys/Packed.json", {'{', '}'}},
        {"eboot.bin", {'o', 'l', 'd'}},
    });
    std::ofstream eboot(root / "app0" / "eboot.bin", std::ios::binary);
    eboot << "disk";
}

void Reads() {
    const auto expected = AppArchiveFixture::Pattern(BigSize);
    const int fd = sceKernelOpen("/app0/data/big.BIN", SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    std::vector<char> buffer(BigSize + 16);
    Check(sceKernelRead(fd, buffer.data(), 70000) == 70000);
    Check(std::equal(buffer.begin(), buffer.begin() + 70000, expected.begin()));
    Check(sceKernelLseek(fd, 0, 1) == 70000);
    Check(lseek_nid_postfix(fd, 0, 1) == 70000);

    std::vector<char> left(10), right(65536);
    const KernelIovec vector[] = {{left.data(), left.size()}, {right.data(), right.size()}};
    Check(sceKernelReadv(fd, vector, 2) == 65546);
    Check(std::equal(left.begin(), left.end(), expected.begin() + 70000) && std::equal(right.begin(), right.end(), expected.begin() + 70010));
    Check(sceKernelPreadv(fd, vector, 2, 5) == 65546);
    Check(std::equal(left.begin(), left.end(), expected.begin() + 5) && std::equal(right.begin(), right.end(), expected.begin() + 15));
    Check(sceKernelLseek(fd, 0, 1) == 135546);

    Check(sceKernelPread(fd, buffer.data(), 100, 65500) == 100 && std::equal(buffer.begin(), buffer.begin() + 100, expected.begin() + 65500));
    Check(pread_nid_postfix(fd, buffer.data(), 100, BigSize - 40) == 40 && std::equal(buffer.begin(), buffer.begin() + 40, expected.end() - 40));
    Check(sceKernelPread(fd, buffer.data(), 1, -1) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelLseek(fd, 0, 1) == 135546);

    int available = 0;
    Check(ioctl_nid_postfix(fd, ReadableRequest, &available) == 0 && available == static_cast<int>(BigSize - 135546));

    Check(sceKernelLseek(fd, -7, 2) == static_cast<std::int64_t>(BigSize - 7));
    Check(sceKernelRead(fd, buffer.data(), 100) == 7 && std::equal(buffer.begin(), buffer.begin() + 7, expected.end() - 7));
    Check(sceKernelRead(fd, buffer.data(), 100) == 0);
    Check(sceKernelLseek(fd, -1, 0) == SCE_KERNEL_ERROR_EINVAL);
    Check(sceKernelLseek(fd, 0, 1) == static_cast<std::int64_t>(BigSize));
    Check(sceKernelWrite(fd, "x", 1) == SCE_KERNEL_ERROR_EBADF);

    FileStat status{};
    Check(sceKernelFstat(fd, &status) == 0);
    Check((status.st_mode & TypeMask) == RegularType && status.st_size == static_cast<std::int64_t>(BigSize) && status.st_nlink == 1);

    KernelAioResult result{-1, 0};
    KernelAioRwRequest request{65530, 20, buffer.data(), &result, fd};
    std::int32_t id = 0;
    std::int32_t state = 0;
    Check(sceKernelAioSubmitReadCommands(&request, 1, 0, &id) == 0);
    Check(sceKernelAioWaitRequest(id, &state, nullptr) == 0);
    Check(result.return_value == 20 && std::equal(buffer.begin(), buffer.begin() + 20, expected.begin() + 65530));
    std::int32_t removed = 0;
    Check(sceKernelAioDeleteRequest(id, &removed) == 0);

    Check(sceKernelLseek(fd, 1000, 0) == 1000);
    const int duplicate = dup_nid_postfix(fd);
    Check(duplicate >= 0 && sceKernelLseek(duplicate, 0, 1) == 1000);
    Check(sceKernelRead(duplicate, buffer.data(), 24) == 24 && sceKernelLseek(fd, 0, 1) == 1024);
    const int above = fcntl_nid_postfix(fd, DuplicateFrom, 200);
    Check(above >= 200 && sceKernelLseek(above, 0, 1) == 1024);
    Check(sceKernelClose(fd) == 0);
    Check(sceKernelRead(duplicate, buffer.data(), 4) == 4 && std::equal(buffer.begin(), buffer.begin() + 4, expected.begin() + 1024));
    Check(sceKernelLseek(above, 0, 1) == 1028);
    Check(close_nid_postfix(above) == 0);

    const int plain = sceKernelOpen("/app0/eboot.bin", SCE_KERNEL_O_RDONLY, 0);
    Check(plain >= 0);
    Check(dup2_nid_postfix(plain, duplicate) == duplicate);
    Check(sceKernelPread(duplicate, buffer.data(), 8, 0) == 4 && std::memcmp(buffer.data(), "disk", 4) == 0);
    Check(sceKernelClose(plain) == 0);
    Check(sceKernelClose(duplicate) == 0);
}

void Status() {
    FileStat status{};
    Check(sceKernelStat("/app0/Data/Small.txt", &status) == 0);
    Check((status.st_mode & TypeMask) == RegularType && (status.st_mode & 0222) == 0 && status.st_size == 3 && status.st_blocks == 1);
    Check(sceKernelStat("/app0/data", &status) == 0 && (status.st_mode & TypeMask) == DirectoryType);
    Check(sceKernelStat("/app0/Data/", &status) == 0);
    Check(sceKernelStat("/app0/Data/Small.txt/", &status) == SCE_KERNEL_ERROR_ENOTDIR);
    Check(sceKernelStat("/app0/Data/Missing.txt", &status) == SCE_KERNEL_ERROR_ENOENT);
    Check(sceKernelStat("/app0/eboot.bin", &status) == 0 && status.st_size == 4);
    Check(sceKernelCheckReachability("/app0/Data/Small.txt") == 0);
    Check(sceKernelCheckReachability("/app0/Data/Missing.txt") == SCE_KERNEL_ERROR_ENOENT);
}

void Refusals() {
    Check(sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDWR, 0) == SCE_KERNEL_ERROR_EROFS);
    Check(sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_TRUNC, 0) == SCE_KERNEL_ERROR_EROFS);
    Check(sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL, 0666) == SCE_KERNEL_ERROR_EEXIST);
    Check(sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0) == SCE_KERNEL_ERROR_ENOTDIR);
    Check(sceKernelOpen("/app0/Data/Missing.txt", SCE_KERNEL_O_RDONLY, 0) == SCE_KERNEL_ERROR_ENOENT);
    Check(NotImplemented([] { (void)sceKernelOpen("/app0/Data", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0); }));
    Check(NotImplemented([] { (void)sceKernelOpen("/app0/sce_sys", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0); }));
    Check(NotImplemented([] { (void)opendir_nid_postfix("/app0/sce_sys"); }));
    Check(NotImplemented([] { (void)opendir_nid_postfix("/app0/Data"); }));
    const int fd = sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    Check(NotImplemented([fd] { (void)fdopen_nid_postfix(fd, "r"); }));
    Check(NotImplemented([fd] { (void)flock_nid_postfix(fd, 1); }));
    Check(NotImplemented([fd] { alignas(8) char statfs[512]; (void)_fstatfs_nid_postfix(fd, statfs); }));
    Check(NotImplemented([fd] { (void)ftruncate_nid_postfix(fd, 0); }));
    Check(NotImplemented([fd] { (void)sceKernelFsync(fd); }));
    Check(NotImplemented([fd] { (void)sceKernelFdatasync(fd); }));
    Check(NotImplemented([fd] { char entries[512]; (void)sceKernelGetdents(fd, entries, sizeof(entries)); }));
    Check(NotImplemented([fd] { (void)sceKernelFchmod(fd, 0444); }));
    Check(NotImplemented([fd] { (void)sceKernelFutimes(fd, nullptr); }));
    FileStat status{};
    Check(sceKernelFstat(fd, &status) == 0 && status.st_size == 3);
    Check(sceKernelClose(fd) == 0);
}

void WithoutArchive() {
    { std::ofstream file(std::filesystem::current_path() / "app0" / "Plain.txt", std::ios::binary); file << "plain"; }
    const int fd = sceKernelOpen("/app0/Plain.txt", SCE_KERNEL_O_RDONLY, 0);
    Check(fd >= 0);
    char buffer[8] = {};
    Check(sceKernelRead(fd, buffer, sizeof(buffer)) == 5 && std::memcmp(buffer, "plain", 5) == 0);
    Check(sceKernelClose(fd) == 0);
    Check(sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDONLY, 0) == SCE_KERNEL_ERROR_ENOENT);
    FileStat status{};
    Check(sceKernelStat("/app0/Data/Small.txt", &status) == SCE_KERNEL_ERROR_ENOENT);
    const int directory = sceKernelOpen("/app0/sce_sys", SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    Check(directory >= 0 && sceKernelClose(directory) == 0);
}

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: guest_kernel_app_archive_tests archive|plain\n");
        return 2;
    }
    const std::string mode = argv[1];
    const auto root = AppArchiveFixture::EnterFreshRoot("anyps5-kernel-app-archive");
    if (mode == "archive") {
        Archive(root);
        Reads();
        Status();
        Refusals();
    } else if (mode == "mmap") {
        Archive(root);
        const int fd = sceKernelOpen("/app0/Data/Small.txt", SCE_KERNEL_O_RDONLY, 0);
        Check(fd >= 0);
        _ZSt13set_terminatePFvvE_nid_postfix(OnMmapRefused);
        (void)mmap_nid_postfix(nullptr, 16384, 1, 2, fd, 0);
        return 1;
    } else if (mode == "plain") {
        WithoutArchive();
    } else {
        return 2;
    }
    std::error_code error;
    std::filesystem::current_path(std::filesystem::temp_directory_path(), error);
    std::filesystem::remove_all(root, error);
    return 0;
}
