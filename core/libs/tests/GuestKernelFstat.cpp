#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <sys/stat.h>
#endif

namespace {

constexpr int GuestEbadf = 9;
constexpr int GuestEfault = 14;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "KERNEL FSTAT: %s (errno %d)\n", message, *__error_nid_postfix());
        std::abort();
    }
}

void AuditStructureLayout() {
    static_assert(sizeof(FileStat) == 120);
    static_assert(sizeof(KernelTimespec) == 16);
    static_assert(offsetof(FileStat, st_dev) == 0);
    static_assert(offsetof(FileStat, st_ino) == 4);
    static_assert(offsetof(FileStat, st_mode) == 8);
    static_assert(offsetof(FileStat, st_nlink) == 10);
    static_assert(offsetof(FileStat, st_uid) == 12);
    static_assert(offsetof(FileStat, st_gid) == 16);
    static_assert(offsetof(FileStat, st_rdev) == 20);
    static_assert(offsetof(FileStat, st_atim) == 24);
    static_assert(offsetof(FileStat, st_mtim) == 40);
    static_assert(offsetof(FileStat, st_ctim) == 56);
    static_assert(offsetof(FileStat, st_size) == 72);
    static_assert(offsetof(FileStat, st_blocks) == 80);
    static_assert(offsetof(FileStat, st_blksize) == 88);
    static_assert(offsetof(FileStat, st_flags) == 92);
    static_assert(offsetof(FileStat, st_gen) == 96);
    static_assert(offsetof(FileStat, st_lspare) == 100);
    static_assert(offsetof(FileStat, st_birthtim) == 104);

    FileStat statRecord{};
    Require(sizeof(statRecord) == 120, "FileStat size must be 120 bytes");
    Require(sizeof(statRecord.st_atim) == 16, "st_atim size must be 16 bytes");
    Require(sizeof(statRecord.st_size) == 8, "st_size size must be 8 bytes");
}

void AuditInvalidDescriptors() {
    const int invalidDescriptors[] = {-1, -2, -100, 65535, 1000000, 1 << 20};
    for (int fd : invalidDescriptors) {
        FileStat sb{};
        *__error_nid_postfix() = 0;
        Require(fstat_nid_disambig1_nid_postfix(fd, &sb) == -1 && *__error_nid_postfix() == GuestEbadf,
            "fstat on invalid descriptor must fail with EBADF (9)");

        *__error_nid_postfix() = 0;
        Require(_fstat_nid_postfix(fd, &sb) == -1 && *__error_nid_postfix() == GuestEbadf,
            "_fstat on invalid descriptor must fail with EBADF (9)");

        Require(sceKernelFstat(fd, &sb) == SCE_KERNEL_ERROR_EBADF,
            "sceKernelFstat on invalid descriptor must return SCE_KERNEL_ERROR_EBADF");

        *__error_nid_postfix() = 0;
        Require(fstat_nid_disambig1_nid_postfix(fd, nullptr) == -1 && *__error_nid_postfix() == GuestEbadf,
            "fstat on invalid descriptor with nullptr must fail with EBADF (9) before EFAULT");

        *__error_nid_postfix() = 0;
        Require(_fstat_nid_postfix(fd, nullptr) == -1 && *__error_nid_postfix() == GuestEbadf,
            "_fstat on invalid descriptor with nullptr must fail with EBADF (9) before EFAULT");

        Require(sceKernelFstat(fd, nullptr) == SCE_KERNEL_ERROR_EBADF,
            "sceKernelFstat on invalid descriptor with nullptr must return SCE_KERNEL_ERROR_EBADF");
    }

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0, "pipe creation failed");
    Require(close_nid_postfix(pipeFds[0]) == 0, "close pipe read-end failed");
    Require(close_nid_postfix(pipeFds[1]) == 0, "close pipe write-end failed");

    FileStat sb{};
    *__error_nid_postfix() = 0;
    Require(fstat_nid_disambig1_nid_postfix(pipeFds[0], &sb) == -1 && *__error_nid_postfix() == GuestEbadf,
        "fstat on closed pipe read-end must fail with EBADF");
    *__error_nid_postfix() = 0;
    Require(fstat_nid_disambig1_nid_postfix(pipeFds[1], &sb) == -1 && *__error_nid_postfix() == GuestEbadf,
        "fstat on closed pipe write-end must fail with EBADF");

    Require(sceKernelFstat(pipeFds[0], &sb) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelFstat on closed pipe read-end must return SCE_KERNEL_ERROR_EBADF");
    Require(sceKernelFstat(pipeFds[1], &sb) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelFstat on closed pipe write-end must return SCE_KERNEL_ERROR_EBADF");
}

void AuditNullBufferOnOpenDescriptors() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0, "pipe creation failed");

    *__error_nid_postfix() = 0;
    Require(fstat_nid_disambig1_nid_postfix(pipeFds[0], nullptr) == -1 && *__error_nid_postfix() == GuestEfault,
        "fstat with nullptr on open pipe read-end must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_fstat_nid_postfix(pipeFds[0], nullptr) == -1 && *__error_nid_postfix() == GuestEfault,
        "_fstat with nullptr on open pipe read-end must fail with EFAULT (14)");
    Require(sceKernelFstat(pipeFds[0], nullptr) == SCE_KERNEL_ERROR_EFAULT,
        "sceKernelFstat with nullptr on open pipe read-end must return SCE_KERNEL_ERROR_EFAULT");

    *__error_nid_postfix() = 0;
    Require(fstat_nid_disambig1_nid_postfix(pipeFds[1], nullptr) == -1 && *__error_nid_postfix() == GuestEfault,
        "fstat with nullptr on open pipe write-end must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_fstat_nid_postfix(pipeFds[1], nullptr) == -1 && *__error_nid_postfix() == GuestEfault,
        "_fstat with nullptr on open pipe write-end must fail with EFAULT (14)");
    Require(sceKernelFstat(pipeFds[1], nullptr) == SCE_KERNEL_ERROR_EFAULT,
        "sceKernelFstat with nullptr on open pipe write-end must return SCE_KERNEL_ERROR_EFAULT");

    Require(close_nid_postfix(pipeFds[0]) == 0, "close pipe read-end failed");
    Require(close_nid_postfix(pipeFds[1]) == 0, "close pipe write-end failed");

    const int sock = socket_nid_postfix(2, 1, 0);
    Require(sock >= 0, "socket creation failed");

    *__error_nid_postfix() = 0;
    Require(fstat_nid_disambig1_nid_postfix(sock, nullptr) == -1 && *__error_nid_postfix() == GuestEfault,
        "fstat with nullptr on open socket must fail with EFAULT (14)");
    *__error_nid_postfix() = 0;
    Require(_fstat_nid_postfix(sock, nullptr) == -1 && *__error_nid_postfix() == GuestEfault,
        "_fstat with nullptr on open socket must fail with EFAULT (14)");
    Require(sceKernelFstat(sock, nullptr) == SCE_KERNEL_ERROR_EFAULT,
        "sceKernelFstat with nullptr on open socket must return SCE_KERNEL_ERROR_EFAULT");

    Require(close_nid_postfix(sock) == 0, "close socket failed");
}

void AuditValidFileDescriptorModeAndSize() {
    const auto timestamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root = std::filesystem::path("anyps5_fstat_test_" + timestamp);
    Require(std::filesystem::create_directory(root), "create fixture directory failed");
    const auto filePath = (root / "fixture.bin").string();

    const char testData[] = "FreeBSD 11.4 POSIX fstat verification payload 2026";
    const auto testDataLen = static_cast<std::int64_t>(sizeof(testData) - 1);
    {
        std::ofstream fileStream(filePath, std::ios::binary);
        fileStream.write(testData, testDataLen);
    }

    const int fd = open_nid_postfix(filePath.c_str(), 0, 0);
    Require(fd >= 0, "open fixture file failed");

    *__error_nid_postfix() = 0;
    Require(fstat_nid_disambig1_nid_postfix(fd, nullptr) == -1 && *__error_nid_postfix() == GuestEfault,
        "fstat with nullptr on open file must fail with EFAULT (14)");
    Require(sceKernelFstat(fd, nullptr) == SCE_KERNEL_ERROR_EFAULT,
        "sceKernelFstat with nullptr on open file must return SCE_KERNEL_ERROR_EFAULT");

    FileStat sb{};
    Require(fstat_nid_disambig1_nid_postfix(fd, &sb) == 0, "fstat on open file failed");
    Require((sb.st_mode & 0170000) == 0100000, "st_mode must indicate S_IFREG (0100000)");
    Require(sb.st_size == testDataLen, "st_size must match expected file size");
    Require(sb.st_nlink >= 1, "st_nlink must be at least 1");
    Require(sb.st_blksize > 0, "st_blksize must be positive");
    Require(sb.st_blocks >= 0, "st_blocks must be non-negative");

    FileStat sbAlias{};
    Require(_fstat_nid_postfix(fd, &sbAlias) == 0, "_fstat on open file failed");
    Require((sbAlias.st_mode & 0170000) == 0100000, "_fstat st_mode must match S_IFREG");
    Require(sbAlias.st_size == testDataLen, "_fstat st_size must match expected size");

    FileStat sbSce{};
    Require(sceKernelFstat(fd, &sbSce) == 0, "sceKernelFstat on open file failed");
    Require((sbSce.st_mode & 0170000) == 0100000, "sceKernelFstat st_mode must match S_IFREG");
    Require(sbSce.st_size == testDataLen, "sceKernelFstat st_size must match expected size");

    Require(close_nid_postfix(fd) == 0, "close fixture file failed");

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

void AuditFileDynamicSizeGrowth() {
    const auto timestamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root = std::filesystem::path("anyps5_fstat_growth_" + timestamp);
    Require(std::filesystem::create_directory(root), "create growth directory failed");
    const auto filePath = (root / "grow.bin").string();

    const int fd = open_nid_postfix(filePath.c_str(), 0x202, 0644);
    Require(fd >= 0, "open growth file failed");

    FileStat initialStat{};
    Require(fstat_nid_disambig1_nid_postfix(fd, &initialStat) == 0, "initial fstat failed");
    Require(initialStat.st_size == 0, "initial size must be 0");
    Require((initialStat.st_mode & 0170000) == 0100000, "initial mode must be regular file");

    const char chunk1[] = "12345678";
    Require(write_nid_postfix(fd, chunk1, 8) == 8, "first write failed");

    FileStat firstGrowthStat{};
    Require(fstat_nid_disambig1_nid_postfix(fd, &firstGrowthStat) == 0, "fstat after first write failed");
    Require(firstGrowthStat.st_size == 8, "st_size must be 8 after first write");

    const char chunk2[] = "abcdefghij";
    Require(write_nid_postfix(fd, chunk2, 10) == 10, "second write failed");

    FileStat secondGrowthStat{};
    Require(sceKernelFstat(fd, &secondGrowthStat) == 0, "sceKernelFstat after second write failed");
    Require(secondGrowthStat.st_size == 18, "st_size must be 18 after second write");

    Require(close_nid_postfix(fd) == 0, "close growth file failed");

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

void AuditPipeAndSocketModes() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0, "pipe creation failed");

    FileStat readEndStat{};
    Require(fstat_nid_disambig1_nid_postfix(pipeFds[0], &readEndStat) == 0, "fstat pipe read-end failed");
    Require((readEndStat.st_mode & 0170000) == 0010000 || (readEndStat.st_mode & 0170000) == 0140000,
        "pipe read-end must have FIFO or socket mode");

    FileStat writeEndStat{};
    Require(_fstat_nid_postfix(pipeFds[1], &writeEndStat) == 0, "_fstat pipe write-end failed");
    Require((writeEndStat.st_mode & 0170000) == 0010000 || (writeEndStat.st_mode & 0170000) == 0140000,
        "pipe write-end must have FIFO or socket mode");

    Require(close_nid_postfix(pipeFds[0]) == 0, "close pipe read-end failed");
    Require(close_nid_postfix(pipeFds[1]) == 0, "close pipe write-end failed");

    const int sock = socket_nid_postfix(2, 1, 0);
    Require(sock >= 0, "socket creation failed");

    FileStat sockStat{};
    Require(fstat_nid_disambig1_nid_postfix(sock, &sockStat) == 0, "fstat socket failed");
    Require((sockStat.st_mode & 0170000) == 0140000, "socket mode must be S_IFSOCK (0140000)");

    Require(close_nid_postfix(sock) == 0, "close socket failed");
}

void AuditNativeFstatModeAttributes() {
    const auto uniqueSuffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const std::filesystem::path tempDir = std::filesystem::path("anyps5-fstat-mode-" + uniqueSuffix);
    std::filesystem::remove_all(tempDir);
    Require(std::filesystem::create_directories(tempDir), "create mode fixture directory failed");
    const std::filesystem::path tempFile = tempDir / "sample.bin";
    const std::string tempFileStr = tempFile.string();
    {
        std::ofstream output(tempFile, std::ios::binary);
        Require(output.is_open(), "create mode fixture file failed");
        output << "abc";
    }
#ifdef _WIN32
    Require(SetFileAttributesW(tempFile.c_str(), FILE_ATTRIBUTE_NORMAL) != 0, "set writable attributes failed");
    FileStat writablePathStat{};
    Require(stat_nid_postfix(tempFileStr.c_str(), &writablePathStat) == 0, "stat writable file failed");
    Require(writablePathStat.st_mode == 0100666, "writable stat mode must be 0100666");

    const int writableFd = open_nid_postfix(tempFileStr.c_str(), 0, 0);
    Require(writableFd >= 0, "open writable file failed");
    FileStat writableFdStat{};
    FileStat writableSceStat{};
    const int writableFdRes = fstat_nid_disambig1_nid_postfix(writableFd, &writableFdStat);
    const int writableSceRes = sceKernelFstat(writableFd, &writableSceStat);
    Require(close_nid_postfix(writableFd) == 0, "close writable file failed");
    Require(writableFdRes == 0, "fstat writable file failed");
    Require(writableSceRes == 0, "sceKernelFstat writable file failed");
    Require(writableFdStat.st_mode == writablePathStat.st_mode, "writable fstat mode must match stat");
    Require(writableFdStat.st_mode == 0100666, "writable fstat mode must be 0100666");
    Require(writableSceStat.st_mode == 0100666, "writable sceKernelFstat mode must be 0100666");

    Require(SetFileAttributesW(tempFile.c_str(), FILE_ATTRIBUTE_READONLY) != 0, "set readonly attributes failed");
    FileStat readonlyPathStat{};
    Require(stat_nid_postfix(tempFileStr.c_str(), &readonlyPathStat) == 0, "stat readonly file failed");
    Require(readonlyPathStat.st_mode == 0100444, "readonly stat mode must be 0100444");
    Require((readonlyPathStat.st_mode & 0222) == 0, "readonly stat mode must clear write bits");

    const int readonlyFd = open_nid_postfix(tempFileStr.c_str(), 0, 0);
    Require(readonlyFd >= 0, "open readonly file failed");
    FileStat readonlyFdStat{};
    FileStat readonlySceStat{};
    const int readonlyFdRes = fstat_nid_disambig1_nid_postfix(readonlyFd, &readonlyFdStat);
    const int readonlySceRes = sceKernelFstat(readonlyFd, &readonlySceStat);
    Require(close_nid_postfix(readonlyFd) == 0, "close readonly file failed");
    Require(SetFileAttributesW(tempFile.c_str(), FILE_ATTRIBUTE_NORMAL) != 0, "restore writable attributes failed");
    Require(readonlyFdRes == 0, "fstat readonly file failed");
    Require(readonlySceRes == 0, "sceKernelFstat readonly file failed");
    Require(readonlyFdStat.st_mode == readonlyPathStat.st_mode, "readonly fstat mode must match stat");
    Require(readonlyFdStat.st_mode == 0100444, "readonly fstat mode must be 0100444");
    Require((readonlyFdStat.st_mode & 0222) == 0, "readonly fstat mode must clear write bits");
    Require(readonlySceStat.st_mode == 0100444, "readonly sceKernelFstat mode must be 0100444");

    const HANDLE dirHandle = CreateFileW(
        tempDir.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);
    Require(dirHandle != INVALID_HANDLE_VALUE, "open native directory handle failed");
    const int dirFd = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(dirHandle), _O_RDONLY);
    if (dirFd < 0) {
        CloseHandle(dirHandle);
        Require(false, "_open_osfhandle on directory handle failed");
    }
    FileStat dirStat{};
    const int dirRes = sceKernelFstat(dirFd, &dirStat);
    ::_close(dirFd);
    Require(dirRes == 0, "sceKernelFstat on native directory descriptor failed");
    Require((dirStat.st_mode & 0170000) == 0040000, "native directory fstat mode must indicate S_IFDIR");
    Require(dirStat.st_blksize == 0x10000, "native directory fstat st_blksize must be 0x10000");
#else
    Require(::chmod(tempFileStr.c_str(), 0444) == 0, "chmod readonly failed");
    FileStat readonlyPathStat{};
    Require(stat_nid_postfix(tempFileStr.c_str(), &readonlyPathStat) == 0, "stat readonly file failed");
    Require((readonlyPathStat.st_mode & 0222) == 0, "readonly stat mode must clear write bits");

    const int readonlyFd = open_nid_postfix(tempFileStr.c_str(), 0, 0);
    Require(readonlyFd >= 0, "open readonly file failed");
    FileStat readonlyFdStat{};
    const int readonlyRes = fstat_nid_disambig1_nid_postfix(readonlyFd, &readonlyFdStat);
    Require(close_nid_postfix(readonlyFd) == 0, "close readonly file failed");
    Require(::chmod(tempFileStr.c_str(), 0644) == 0, "chmod restore failed");
    Require(readonlyRes == 0, "fstat readonly file failed");
    Require(readonlyFdStat.st_mode == readonlyPathStat.st_mode, "readonly fstat mode must match stat");
    Require((readonlyFdStat.st_mode & 0222) == 0, "readonly fstat mode must clear write bits");
#endif
    std::error_code removeError;
    std::filesystem::remove_all(tempDir, removeError);
    Require(!removeError, "remove mode fixture directory failed");
}

} // namespace

int main() {
    AuditStructureLayout();
    AuditInvalidDescriptors();
    AuditNullBufferOnOpenDescriptors();
    AuditValidFileDescriptorModeAndSize();
    AuditFileDynamicSizeGrowth();
    AuditPipeAndSocketModes();
    AuditNativeFstatModeAttributes();
    std::puts("GuestKernelFstat tests passed");
    return 0;
}
