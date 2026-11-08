#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <source_location>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
#else
#include <unistd.h>
#endif

namespace {

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;
constexpr int GuestEspipe = 29;

#ifdef _WIN32
void IgnoreParameterReport(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {
}
#endif

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "GuestKernelLseek check failed at line %u\n", location.line());
        std::abort();
    }
}

void RequirePosixFailure(std::int64_t result, int expectedError, std::source_location location = std::source_location::current()) {
    Require(result == -1, location);
    Require(*__error_nid_postfix() == expectedError, location);
}

void AuditInvalidDescriptors() {
    for (const int fd : {-1000, -100, -2, -1, 1024, 65535, std::numeric_limits<int>::max()}) {
        RequirePosixFailure(lseek_nid_postfix(fd, 0, SEEK_SET), GuestEbadf);
        RequirePosixFailure(lseek_nid_postfix(fd, 0, SEEK_CUR), GuestEbadf);
        RequirePosixFailure(lseek_nid_postfix(fd, 0, SEEK_END), GuestEbadf);
        Require(sceKernelLseek(fd, 0, SEEK_SET) == SCE_KERNEL_ERROR_EBADF);
        Require(sceKernelLseek(fd, 0, SEEK_CUR) == SCE_KERNEL_ERROR_EBADF);
        Require(sceKernelLseek(fd, 0, SEEK_END) == SCE_KERNEL_ERROR_EBADF);
        RequirePosixFailure(lseek_nid_postfix(fd, 10, SEEK_SET), GuestEbadf);
        Require(sceKernelLseek(fd, 10, SEEK_SET) == SCE_KERNEL_ERROR_EBADF);
        RequirePosixFailure(lseek_nid_postfix(fd, -5, SEEK_SET), GuestEbadf);
        Require(sceKernelLseek(fd, -5, SEEK_SET) == SCE_KERNEL_ERROR_EBADF);
        RequirePosixFailure(lseek_nid_postfix(fd, 0, 99), GuestEbadf);
        Require(sceKernelLseek(fd, 0, 99) == SCE_KERNEL_ERROR_EBADF);
    }
}

void AuditInvalidWhence(int fd) {
    for (const int whence : {-1000, -100, -2, -1, 3, 4, 10, 99, 1000, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        RequirePosixFailure(lseek_nid_postfix(fd, 0, whence), GuestEinval);
        RequirePosixFailure(lseek_nid_postfix(fd, 10, whence), GuestEinval);
        RequirePosixFailure(lseek_nid_postfix(fd, -5, whence), GuestEinval);
        bool rejected = false;
        try {
            const auto result = sceKernelLseek(fd, 0, whence);
            if (result == SCE_KERNEL_ERROR_EINVAL) rejected = true;
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Require(rejected);
    }
}

void AuditNegativeOffsets(int fd, std::int64_t fileSize) {
    for (const std::int64_t off : {std::int64_t{-1}, std::int64_t{-2}, std::int64_t{-5}, std::int64_t{-10}, std::int64_t{-100}, std::int64_t{-4096}, std::numeric_limits<std::int64_t>::min()}) {
        RequirePosixFailure(lseek_nid_postfix(fd, off, SEEK_SET), GuestEinval);
        Require(sceKernelLseek(fd, off, SEEK_SET) == SCE_KERNEL_ERROR_EINVAL);
    }

    Require(lseek_nid_postfix(fd, 5, SEEK_SET) == 5);
    Require(sceKernelLseek(fd, 0, SEEK_CUR) == 5);
    for (const std::int64_t off : {std::int64_t{-6}, std::int64_t{-7}, std::int64_t{-10}, std::int64_t{-100}, std::int64_t{-4096}, std::numeric_limits<std::int64_t>::min()}) {
        RequirePosixFailure(lseek_nid_postfix(fd, off, SEEK_CUR), GuestEinval);
        Require(sceKernelLseek(fd, off, SEEK_CUR) == SCE_KERNEL_ERROR_EINVAL);
        Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == 5);
        Require(sceKernelLseek(fd, 0, SEEK_CUR) == 5);
    }

    for (const std::int64_t off : {-(fileSize + 1), -(fileSize + 2), -(fileSize + 10), -(fileSize + 100)}) {
        RequirePosixFailure(lseek_nid_postfix(fd, off, SEEK_END), GuestEinval);
        Require(sceKernelLseek(fd, off, SEEK_END) == SCE_KERNEL_ERROR_EINVAL);
        Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == 5);
        Require(sceKernelLseek(fd, 0, SEEK_CUR) == 5);
    }
}

void AuditNonSeekableDescriptors() {
    const int sock = socket_nid_postfix(2, 1, 0);
    if (sock >= 0) {
        RequirePosixFailure(lseek_nid_postfix(sock, 0, SEEK_SET), GuestEspipe);
        Require(sceKernelLseek(sock, 0, SEEK_SET) == SCE_KERNEL_ERROR_ESPIPE);
        RequirePosixFailure(lseek_nid_postfix(sock, 0, SEEK_CUR), GuestEspipe);
        Require(sceKernelLseek(sock, 0, SEEK_CUR) == SCE_KERNEL_ERROR_ESPIPE);
        RequirePosixFailure(lseek_nid_postfix(sock, 0, SEEK_END), GuestEspipe);
        Require(sceKernelLseek(sock, 0, SEEK_END) == SCE_KERNEL_ERROR_ESPIPE);
        RequirePosixFailure(lseek_nid_postfix(sock, 10, SEEK_SET), GuestEspipe);
        Require(sceKernelLseek(sock, 10, SEEK_SET) == SCE_KERNEL_ERROR_ESPIPE);
        RequirePosixFailure(lseek_nid_postfix(sock, -5, SEEK_SET), GuestEspipe);
        Require(sceKernelLseek(sock, -5, SEEK_SET) == SCE_KERNEL_ERROR_ESPIPE);
        Require(close_nid_postfix(sock) == 0);
        RequirePosixFailure(lseek_nid_postfix(sock, 0, SEEK_SET), GuestEbadf);
        Require(sceKernelLseek(sock, 0, SEEK_SET) == SCE_KERNEL_ERROR_EBADF);
    }

    int pipeDescriptors[2]{-1, -1};
    if (pipe_nid_postfix(pipeDescriptors) == 0) {
        for (const int pfd : pipeDescriptors) {
            RequirePosixFailure(lseek_nid_postfix(pfd, 0, SEEK_SET), GuestEspipe);
            Require(sceKernelLseek(pfd, 0, SEEK_SET) == SCE_KERNEL_ERROR_ESPIPE);
            RequirePosixFailure(lseek_nid_postfix(pfd, 5, SEEK_CUR), GuestEspipe);
            Require(sceKernelLseek(pfd, 5, SEEK_CUR) == SCE_KERNEL_ERROR_ESPIPE);
            RequirePosixFailure(lseek_nid_postfix(pfd, 0, SEEK_END), GuestEspipe);
            Require(sceKernelLseek(pfd, 0, SEEK_END) == SCE_KERNEL_ERROR_ESPIPE);
            RequirePosixFailure(lseek_nid_postfix(pfd, -1, SEEK_SET), GuestEspipe);
            Require(sceKernelLseek(pfd, -1, SEEK_SET) == SCE_KERNEL_ERROR_ESPIPE);
            Require(close_nid_postfix(pfd) == 0);
            RequirePosixFailure(lseek_nid_postfix(pfd, 0, SEEK_SET), GuestEbadf);
            Require(sceKernelLseek(pfd, 0, SEEK_SET) == SCE_KERNEL_ERROR_EBADF);
        }
    }
}

void AuditValidSeekingAndEquivalence(int fd, std::int64_t fileSize) {
    Require(lseek_nid_postfix(fd, 0, SEEK_SET) == 0);
    Require(sceKernelLseek(fd, 0, SEEK_SET) == 0);
    Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == 0);
    Require(sceKernelLseek(fd, 0, SEEK_CUR) == 0);

    Require(lseek_nid_postfix(fd, 8, SEEK_SET) == 8);
    Require(sceKernelLseek(fd, 0, SEEK_CUR) == 8);

    Require(sceKernelLseek(fd, 16, SEEK_SET) == 16);
    Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == 16);

    Require(lseek_nid_postfix(fd, 4, SEEK_CUR) == 20);
    Require(sceKernelLseek(fd, 0, SEEK_CUR) == 20);

    Require(sceKernelLseek(fd, -6, SEEK_CUR) == 14);
    Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == 14);

    char verifyBuffer[4]{};
    Require(read_nid_postfix(fd, verifyBuffer, 4) == 4);
    Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == 18);
    Require(sceKernelLseek(fd, 0, SEEK_CUR) == 18);

    Require(lseek_nid_postfix(fd, 0, SEEK_END) == fileSize);
    Require(sceKernelLseek(fd, 0, SEEK_END) == fileSize);
    Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == fileSize);
    Require(sceKernelLseek(fd, 0, SEEK_CUR) == fileSize);

    Require(lseek_nid_postfix(fd, -12, SEEK_END) == fileSize - 12);
    Require(sceKernelLseek(fd, -12, SEEK_END) == fileSize - 12);
    Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == fileSize - 12);
    Require(sceKernelLseek(fd, 0, SEEK_CUR) == fileSize - 12);

    Require(lseek_nid_postfix(fd, 50, SEEK_END) == fileSize + 50);
    Require(sceKernelLseek(fd, 50, SEEK_END) == fileSize + 50);
    Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == fileSize + 50);
    Require(sceKernelLseek(fd, 0, SEEK_CUR) == fileSize + 50);

    Require(lseek_nid_postfix(fd, 500, SEEK_SET) == 500);
    Require(sceKernelLseek(fd, 500, SEEK_SET) == 500);
    Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == 500);
    Require(sceKernelLseek(fd, 0, SEEK_CUR) == 500);

    Require(sceKernelLseek(fd, 0, SEEK_SET) == 0);
    Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == 0);
}

} // namespace

int main() {
#ifdef _WIN32
    _set_thread_local_invalid_parameter_handler(IgnoreParameterReport);
#endif

    AuditInvalidDescriptors();
    AuditNonSeekableDescriptors();

    const auto root = std::filesystem::path("anyps5-kernel-lseek-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto path = (root / "lseek_data.bin").string();

    struct Cleaner {
        std::filesystem::path directory;
        ~Cleaner() {
            std::error_code ec;
            std::filesystem::remove_all(directory, ec);
        }
    } cleaner{root};

    const int fd = open_nid_postfix(path.c_str(), SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC, 0666);
    Require(fd >= 0);

    const char payload[32] = "0123456789ABCDEFGHIJKLMNOPQRSTU";
    Require(write_nid_postfix(fd, payload, 32) == 32);
    Require(std::filesystem::file_size(path) == 32);

    AuditInvalidWhence(fd);
    AuditNegativeOffsets(fd, 32);
    AuditValidSeekingAndEquivalence(fd, 32);

    Require(close_nid_postfix(fd) == 0);

    RequirePosixFailure(lseek_nid_postfix(fd, 0, SEEK_SET), GuestEbadf);
    Require(sceKernelLseek(fd, 0, SEEK_SET) == SCE_KERNEL_ERROR_EBADF);

    return 0;
}
