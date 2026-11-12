#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace {

constexpr int PosixEbadf = 9;
constexpr int PosixEfault = 14;
constexpr int PosixEisdir = 21;
constexpr int PosixEinval = 22;
constexpr int PosixEspipe = 29;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", message, *__error_nid_postfix());
    std::abort();
}

void AuditNegativeOffsetRejection(int descriptor) {
    char buffer[16]{};

    Require(pread_nid_postfix(descriptor, buffer, 1, -1) == -1 && *__error_nid_postfix() == PosixEinval,
        "pread negative offset -1 must fail with EINVAL (22)");
    Require(pread_nid_postfix(descriptor, buffer, 1, -100) == -1 && *__error_nid_postfix() == PosixEinval,
        "pread negative offset -100 must fail with EINVAL (22)");
    Require(pread_nid_postfix(descriptor, buffer, 1, std::numeric_limits<std::int64_t>::min()) == -1 && *__error_nid_postfix() == PosixEinval,
        "pread minimum int64 offset must fail with EINVAL (22)");
    Require(pread_nid_postfix(descriptor, nullptr, 0, -1) == -1 && *__error_nid_postfix() == PosixEinval,
        "pread zero-length with negative offset must fail with EINVAL (22)");
    Require(pread_nid_postfix(descriptor, nullptr, 1, -1) == -1 && *__error_nid_postfix() == PosixEinval,
        "pread null pointer with negative offset must fail with EINVAL (22)");

    Require(pwrite_nid_disambig1_nid_postfix(descriptor, "A", 1, -1) == -1 && *__error_nid_postfix() == PosixEinval,
        "pwrite negative offset -1 must fail with EINVAL (22)");
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, "A", 1, -100) == -1 && *__error_nid_postfix() == PosixEinval,
        "pwrite negative offset -100 must fail with EINVAL (22)");
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, "A", 1, std::numeric_limits<std::int64_t>::min()) == -1 && *__error_nid_postfix() == PosixEinval,
        "pwrite minimum int64 offset must fail with EINVAL (22)");
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, nullptr, 0, -1) == -1 && *__error_nid_postfix() == PosixEinval,
        "pwrite zero-length with negative offset must fail with EINVAL (22)");
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, nullptr, 1, -1) == -1 && *__error_nid_postfix() == PosixEinval,
        "pwrite null pointer with negative offset must fail with EINVAL (22)");

    Require(sceKernelPread(descriptor, buffer, 1, -1) == SCE_KERNEL_ERROR_EINVAL,
        "sceKernelPread negative offset -1 must return SCE_KERNEL_ERROR_EINVAL");
    Require(sceKernelPread(descriptor, buffer, 1, -100) == SCE_KERNEL_ERROR_EINVAL,
        "sceKernelPread negative offset -100 must return SCE_KERNEL_ERROR_EINVAL");
    Require(sceKernelPread(descriptor, buffer, 1, std::numeric_limits<std::int64_t>::min()) == SCE_KERNEL_ERROR_EINVAL,
        "sceKernelPread minimum int64 offset must return SCE_KERNEL_ERROR_EINVAL");
    Require(sceKernelPread(descriptor, nullptr, 0, -1) == SCE_KERNEL_ERROR_EINVAL,
        "sceKernelPread zero-length with negative offset must return SCE_KERNEL_ERROR_EINVAL");

    Require(sceKernelPwrite(descriptor, "A", 1, -1) == SCE_KERNEL_ERROR_EINVAL,
        "sceKernelPwrite negative offset -1 must return SCE_KERNEL_ERROR_EINVAL");
    Require(sceKernelPwrite(descriptor, "A", 1, -100) == SCE_KERNEL_ERROR_EINVAL,
        "sceKernelPwrite negative offset -100 must return SCE_KERNEL_ERROR_EINVAL");
    Require(sceKernelPwrite(descriptor, "A", 1, std::numeric_limits<std::int64_t>::min()) == SCE_KERNEL_ERROR_EINVAL,
        "sceKernelPwrite minimum int64 offset must return SCE_KERNEL_ERROR_EINVAL");
    Require(sceKernelPwrite(descriptor, nullptr, 0, -1) == SCE_KERNEL_ERROR_EINVAL,
        "sceKernelPwrite zero-length with negative offset must return SCE_KERNEL_ERROR_EINVAL");
}

void AuditDescriptorValidation(const std::string& readOnlyPath, const std::string& writeOnlyPath) {
    char buffer[16]{};

    Require(pread_nid_postfix(-1, buffer, 1, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pread on fd -1 must fail with EBADF (9)");
    Require(pread_nid_postfix(-1, nullptr, 0, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pread zero-length on fd -1 must fail with EBADF (9)");
    Require(pread_nid_postfix(-1, nullptr, 1, -1) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pread on fd -1 with negative offset must prioritize EBADF (9)");
    Require(pread_nid_postfix(1 << 20, buffer, 1, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pread on unopened fd must fail with EBADF (9)");

    Require(pwrite_nid_disambig1_nid_postfix(-1, "A", 1, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pwrite on fd -1 must fail with EBADF (9)");
    Require(pwrite_nid_disambig1_nid_postfix(-1, nullptr, 0, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pwrite zero-length on fd -1 must fail with EBADF (9)");
    Require(pwrite_nid_disambig1_nid_postfix(-1, nullptr, 1, -1) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pwrite on fd -1 with negative offset must prioritize EBADF (9)");
    Require(pwrite_nid_disambig1_nid_postfix(1 << 20, "A", 1, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pwrite on unopened fd must fail with EBADF (9)");

    Require(sceKernelPread(-1, buffer, 1, 0) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPread on fd -1 must return SCE_KERNEL_ERROR_EBADF");
    Require(sceKernelPread(-1, nullptr, 0, 0) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPread zero-length on fd -1 must return SCE_KERNEL_ERROR_EBADF");
    Require(sceKernelPread(-1, nullptr, 1, -1) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPread on fd -1 with negative offset must prioritize SCE_KERNEL_ERROR_EBADF");
    Require(sceKernelPread(1 << 20, buffer, 1, 0) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPread on unopened fd must return SCE_KERNEL_ERROR_EBADF");

    Require(sceKernelPwrite(-1, "A", 1, 0) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPwrite on fd -1 must return SCE_KERNEL_ERROR_EBADF");
    Require(sceKernelPwrite(-1, nullptr, 0, 0) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPwrite zero-length on fd -1 must return SCE_KERNEL_ERROR_EBADF");
    Require(sceKernelPwrite(-1, nullptr, 1, -1) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPwrite on fd -1 with negative offset must prioritize SCE_KERNEL_ERROR_EBADF");
    Require(sceKernelPwrite(1 << 20, "A", 1, 0) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPwrite on unopened fd must return SCE_KERNEL_ERROR_EBADF");

    const int readOnlyFd = open_nid_postfix(readOnlyPath.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(readOnlyFd >= 0, "open readOnly failed");
    Require(pwrite_nid_disambig1_nid_postfix(readOnlyFd, "A", 1, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pwrite on read-only fd must fail with EBADF (9)");
    Require(pwrite_nid_disambig1_nid_postfix(readOnlyFd, nullptr, 1, -1) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pwrite on read-only fd with negative offset must prioritize EBADF (9)");
    Require(sceKernelPwrite(readOnlyFd, "A", 1, 0) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPwrite on read-only fd must return SCE_KERNEL_ERROR_EBADF");
    Require(close_nid_postfix(readOnlyFd) == 0, "close readOnly failed");

    Require(pread_nid_postfix(readOnlyFd, buffer, 1, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pread on closed fd must fail with EBADF (9)");
    Require(pwrite_nid_disambig1_nid_postfix(readOnlyFd, "A", 1, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pwrite on closed fd must fail with EBADF (9)");
    Require(sceKernelPread(readOnlyFd, buffer, 1, 0) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPread on closed fd must return SCE_KERNEL_ERROR_EBADF");
    Require(sceKernelPwrite(readOnlyFd, "A", 1, 0) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPwrite on closed fd must return SCE_KERNEL_ERROR_EBADF");

    const int writeOnlyFd = open_nid_postfix(writeOnlyPath.c_str(), SCE_KERNEL_O_WRONLY, 0);
    Require(writeOnlyFd >= 0, "open writeOnly failed");
    Require(pread_nid_postfix(writeOnlyFd, buffer, 1, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pread on write-only fd must fail with EBADF (9)");
    Require(pread_nid_postfix(writeOnlyFd, nullptr, 1, -1) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pread on write-only fd with negative offset must prioritize EBADF (9)");
    Require(sceKernelPread(writeOnlyFd, buffer, 1, 0) == SCE_KERNEL_ERROR_EBADF,
        "sceKernelPread on write-only fd must return SCE_KERNEL_ERROR_EBADF");
    Require(close_nid_postfix(writeOnlyFd) == 0, "close writeOnly failed");

    const int sock = socket_nid_postfix(2, 1, 0);
    Require(sock >= 0, "socket failed");
    Require(pread_nid_postfix(sock, buffer, 1, 0) == -1 && *__error_nid_postfix() == PosixEspipe,
        "pread on socket must fail with ESPIPE (29)");
    Require(pwrite_nid_disambig1_nid_postfix(sock, "A", 1, 0) == -1 && *__error_nid_postfix() == PosixEspipe,
        "pwrite on socket must fail with ESPIPE (29)");
    Require(sceKernelPread(sock, buffer, 1, 0) == SCE_KERNEL_ERROR_ESPIPE,
        "sceKernelPread on socket must return SCE_KERNEL_ERROR_ESPIPE");
    Require(sceKernelPwrite(sock, "A", 1, 0) == SCE_KERNEL_ERROR_ESPIPE,
        "sceKernelPwrite on socket must return SCE_KERNEL_ERROR_ESPIPE");
    Require(close_nid_postfix(sock) == 0, "close socket failed");
    Require(pread_nid_postfix(sock, buffer, 1, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pread on closed socket must fail with EBADF (9)");
    Require(pwrite_nid_disambig1_nid_postfix(sock, "A", 1, 0) == -1 && *__error_nid_postfix() == PosixEbadf,
        "pwrite on closed socket must fail with EBADF (9)");

    const int nullFd = open_nid_postfix("/dev/null", SCE_KERNEL_O_RDWR, 0);
    Require(nullFd >= 0, "open /dev/null failed");
    char nullBuffer[16]{};
    Require(sceKernelPread(nullFd, nullBuffer, sizeof(nullBuffer), 0) == 0,
        "sceKernelPread on /dev/null must return 0");
    Require(sceKernelPwrite(nullFd, nullBuffer, sizeof(nullBuffer), 0) == static_cast<std::int64_t>(sizeof(nullBuffer)),
        "sceKernelPwrite on /dev/null must return the length");
    Require(close_nid_postfix(nullFd) == 0, "close /dev/null failed");
}

void AuditNullBufferValidation(int descriptor) {
    Require(pread_nid_postfix(descriptor, nullptr, 1, 0) == -1 && *__error_nid_postfix() == PosixEfault,
        "pread with null buffer and nbytes > 0 must fail with EFAULT (14)");
    Require(pread_nid_postfix(descriptor, nullptr, 64, 0) == -1 && *__error_nid_postfix() == PosixEfault,
        "pread with null buffer and nbytes=64 must fail with EFAULT (14)");
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, nullptr, 1, 0) == -1 && *__error_nid_postfix() == PosixEfault,
        "pwrite with null buffer and nbytes > 0 must fail with EFAULT (14)");
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, nullptr, 64, 0) == -1 && *__error_nid_postfix() == PosixEfault,
        "pwrite with null buffer and nbytes=64 must fail with EFAULT (14)");

    Require(sceKernelPread(descriptor, nullptr, 1, 0) == SCE_KERNEL_ERROR_EFAULT,
        "sceKernelPread with null buffer must return SCE_KERNEL_ERROR_EFAULT");
    Require(sceKernelPwrite(descriptor, nullptr, 1, 0) == SCE_KERNEL_ERROR_EFAULT,
        "sceKernelPwrite with null buffer must return SCE_KERNEL_ERROR_EFAULT");

    Require(pread_nid_postfix(descriptor, nullptr, 0, 0) == 0,
        "pread with null buffer and nbytes=0 must succeed returning 0");
    Require(pread_nid_postfix(descriptor, nullptr, 0, 10) == 0,
        "pread with null buffer and nbytes=0 at offset 10 must succeed returning 0");
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, nullptr, 0, 0) == 0,
        "pwrite with null buffer and nbytes=0 must succeed returning 0");
    Require(pwrite_nid_disambig1_nid_postfix(descriptor, nullptr, 0, 10) == 0,
        "pwrite with null buffer and nbytes=0 at offset 10 must succeed returning 0");

    Require(sceKernelPread(descriptor, nullptr, 0, 0) == 0,
        "sceKernelPread with null buffer and nbytes=0 must succeed returning 0");
    Require(sceKernelPread(descriptor, nullptr, 0, 10) == 0,
        "sceKernelPread with null buffer and nbytes=0 at offset 10 must succeed returning 0");
    Require(sceKernelPwrite(descriptor, nullptr, 0, 0) == 0,
        "sceKernelPwrite with null buffer and nbytes=0 must succeed returning 0");
    Require(sceKernelPwrite(descriptor, nullptr, 0, 10) == 0,
        "sceKernelPwrite with null buffer and nbytes=0 at offset 10 must succeed returning 0");
}

void AuditOffsetIndependenceFromCursor(int descriptor) {
    Require(lseek_nid_postfix(descriptor, 4, SEEK_SET) == 4, "lseek to 4 failed");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_CUR) == 4, "verify cursor at 4 failed");

    char chunk[8]{};
    Require(pread_nid_postfix(descriptor, chunk, 4, 10) == 4, "pread 4 bytes from offset 10 failed");
    Require(std::memcmp(chunk, "ABCD", 4) == 0, "pread returned unexpected bytes");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_CUR) == 4, "cursor modified by pread");

    Require(pwrite_nid_disambig1_nid_postfix(descriptor, "wxyz", 4, 0) == 4, "pwrite 4 bytes to offset 0 failed");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_CUR) == 4, "cursor modified by pwrite");

    Require(pread_nid_postfix(descriptor, chunk, 4, 0) == 4, "pread 4 bytes from offset 0 failed");
    Require(std::memcmp(chunk, "wxyz", 4) == 0, "pread offset 0 returned unexpected bytes");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_CUR) == 4, "cursor modified by second pread");

    char cursorRead[4]{};
    Require(read_nid_postfix(descriptor, cursorRead, 2) == 2, "read from cursor failed");
    Require(std::memcmp(cursorRead, "45", 2) == 0, "read from cursor returned unexpected bytes");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_CUR) == 6, "cursor not advanced by read");

    Require(pwrite_nid_disambig1_nid_postfix(descriptor, "!!", 2, 8) == 2, "pwrite at offset 8 failed");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_CUR) == 6, "cursor modified by pwrite at 8");

    Require(sceKernelPread(descriptor, chunk, 4, 0) == 4, "sceKernelPread at offset 0 failed");
    Require(std::memcmp(chunk, "wxyz", 4) == 0, "sceKernelPread returned unexpected bytes");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_CUR) == 6, "cursor modified by sceKernelPread");

    Require(sceKernelPwrite(descriptor, "##", 2, 14) == 2, "sceKernelPwrite at offset 14 failed");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_CUR) == 6, "cursor modified by sceKernelPwrite");

    Require(write_nid_postfix(descriptor, "QQ", 2) == 2, "write to cursor failed");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_CUR) == 8, "cursor not advanced by write");

    char full[17]{};
    Require(pread_nid_postfix(descriptor, full, 16, 0) == 16, "pread full contents failed");
    Require(std::memcmp(full, "wxyz45QQ!!ABCD##", 16) == 0, "file contents mismatch after positioned writes");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_CUR) == 8, "cursor modified by final pread");

    Require(pread_nid_postfix(descriptor, chunk, 4, 16) == 0, "pread at EOF must return 0");
    Require(pread_nid_postfix(descriptor, chunk, 4, 100) == 0, "pread beyond EOF must return 0");
    Require(sceKernelPread(descriptor, chunk, 4, 16) == 0, "sceKernelPread at EOF must return 0");
    Require(sceKernelPread(descriptor, chunk, 4, 100) == 0, "sceKernelPread beyond EOF must return 0");
    Require(lseek_nid_postfix(descriptor, 0, SEEK_CUR) == 8, "cursor modified by pread at/beyond EOF");
}

void AuditDirectoryAndLimitBounds(const std::string& directoryPath, int regularFd) {
    char buffer[16]{};
    const int dirFd = open_nid_postfix(directoryPath.c_str(), SCE_KERNEL_O_RDONLY | SCE_KERNEL_O_DIRECTORY, 0);
    if (dirFd >= 0) {
        Require(pread_nid_postfix(dirFd, buffer, 1, 0) == -1 && *__error_nid_postfix() == PosixEisdir,
            "pread on directory descriptor must fail with EISDIR (21)");
        Require(sceKernelPread(dirFd, buffer, 1, 0) == SCE_KERNEL_ERROR_EISDIR,
            "sceKernelPread on directory descriptor must return SCE_KERNEL_ERROR_EISDIR");
        Require(close_nid_postfix(dirFd) == 0, "close directory fd failed");
    }

    constexpr auto oversized = static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) + 1;
    Require(pread_nid_postfix(regularFd, buffer, oversized, 0) == -1 && *__error_nid_postfix() == PosixEinval,
        "pread oversized nbytes must fail with EINVAL (22)");
    Require(pwrite_nid_disambig1_nid_postfix(regularFd, "A", oversized, 0) == -1 && *__error_nid_postfix() == PosixEinval,
        "pwrite oversized nbytes must fail with EINVAL (22)");
}

} // namespace

int main() {
    const auto root = std::filesystem::path("anyps5-pread-pwrite-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root), "create temporary test root failed");

    const auto dataFile = root / "data.bin";
    const auto roFile = root / "readonly.bin";
    const auto woFile = root / "writeonly.bin";
    const auto subDir = root / "subdir";

    {
        std::ofstream stream(dataFile, std::ios::binary);
        stream << "0123456789ABCDEF";
        Require(stream.good(), "write data.bin failed");
    }
    {
        std::ofstream stream(roFile, std::ios::binary);
        stream << "RO";
        Require(stream.good(), "write readonly.bin failed");
    }
    {
        std::ofstream stream(woFile, std::ios::binary);
        stream << "WO";
        Require(stream.good(), "write writeonly.bin failed");
    }
    Require(std::filesystem::create_directory(subDir), "create subdir failed");

    const int fd = open_nid_postfix(dataFile.string().c_str(), SCE_KERNEL_O_RDWR, 0);
    Require(fd >= 0, "open dataFile failed");

    AuditNegativeOffsetRejection(fd);
    AuditDescriptorValidation(roFile.string(), woFile.string());
    AuditNullBufferValidation(fd);
    AuditOffsetIndependenceFromCursor(fd);
    AuditDirectoryAndLimitBounds(subDir.string(), fd);

    Require(close_nid_postfix(fd) == 0, "close dataFile failed");

    std::filesystem::remove_all(root);
    return 0;
}
