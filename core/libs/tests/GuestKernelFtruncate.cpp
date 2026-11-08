#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
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

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "GuestKernelFtruncate check failed at line %u\n", location.line());
        std::abort();
    }
}

static void RequireFailure(int result, int error, std::source_location location = std::source_location::current()) {
    Require(result == -1, location);
    Require(*__error_nid_postfix() == error, location);
}

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;
constexpr int SceKernelErrorBase = static_cast<int>(0x80020000u);

#ifdef _WIN32
static unsigned parameterReports = 0;
static void CountParameterReport(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {
    ++parameterReports;
}
#endif

int main() {
    const auto root = std::filesystem::path("anyps5-kernel-ftruncate-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto path = (root / "truncate_data.bin").string();

    struct Cleaner {
        std::filesystem::path directory;
        ~Cleaner() {
            std::error_code ec;
            std::filesystem::remove_all(directory, ec);
        }
    } cleaner{root};

    const int initialFd = open_nid_postfix(path.c_str(), SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC, 0666);
    Require(initialFd >= 0);
    Require(close_nid_postfix(initialFd) == 0);

    for (const int fd : {-100, -2, -1, initialFd, 1024, 65535, std::numeric_limits<int>::max()}) {
        RequireFailure(ftruncate_nid_postfix(fd, 0), GuestEbadf);
        RequireFailure(ftruncate_nid_postfix(fd, 16), GuestEbadf);
        Require(sceKernelFtruncate(fd, 0) == SceKernelErrorBase + GuestEbadf);
        Require(sceKernelFtruncate(fd, 16) == SceKernelErrorBase + GuestEbadf);
    }

#ifdef _WIN32
    const auto previousHandler = _set_thread_local_invalid_parameter_handler(CountParameterReport);
    RequireFailure(ftruncate_nid_postfix(-1, 0), GuestEbadf);
    RequireFailure(ftruncate_nid_postfix(initialFd, 0), GuestEbadf);
    Require(_set_thread_local_invalid_parameter_handler(previousHandler) == CountParameterReport);
    Require(parameterReports == 0);
#endif

    const int fd = open_nid_postfix(path.c_str(), SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC, 0666);
    Require(fd >= 0);

    for (const std::int64_t negativeLength : {std::int64_t{-1}, std::int64_t{-2}, std::int64_t{-100}, std::int64_t{-4096}, std::numeric_limits<std::int64_t>::min()}) {
        RequireFailure(ftruncate_nid_postfix(fd, negativeLength), GuestEinval);
        Require(sceKernelFtruncate(fd, negativeLength) == SceKernelErrorBase + GuestEinval);
    }

    RequireFailure(ftruncate_nid_postfix(-1, -1), GuestEinval);
    Require(sceKernelFtruncate(-1, -1) == SceKernelErrorBase + GuestEinval);

    const char initialPayload[17] = "0123456789ABCDEF";
    Require(write_nid_postfix(fd, initialPayload, 16) == 16);
    Require(std::filesystem::file_size(path) == 16);

    const int roFd = open_nid_postfix(path.c_str(), SCE_KERNEL_O_RDONLY, 0);
    Require(roFd >= 0);
    for (const std::int64_t len : {std::int64_t{0}, std::int64_t{8}, std::int64_t{16}, std::int64_t{32}}) {
        RequireFailure(ftruncate_nid_postfix(roFd, len), GuestEinval);
        Require(sceKernelFtruncate(roFd, len) == SceKernelErrorBase + GuestEinval);
    }
    Require(close_nid_postfix(roFd) == 0);

    Require(ftruncate_nid_postfix(fd, 32) == 0);
    Require(std::filesystem::file_size(path) == 32);

    char buffer32[32]{};
    Require(lseek_nid_postfix(fd, 0, SEEK_SET) == 0);
    Require(read_nid_postfix(fd, buffer32, 32) == 32);
    Require(std::memcmp(buffer32, initialPayload, 16) == 0);
    for (std::size_t i = 16; i < 32; ++i) {
        Require(buffer32[i] == 0);
    }

    Require(sceKernelFtruncate(fd, 64) == 0);
    Require(std::filesystem::file_size(path) == 64);

    char buffer64[64]{};
    Require(lseek_nid_postfix(fd, 0, SEEK_SET) == 0);
    Require(read_nid_postfix(fd, buffer64, 64) == 64);
    Require(std::memcmp(buffer64, initialPayload, 16) == 0);
    for (std::size_t i = 16; i < 64; ++i) {
        Require(buffer64[i] == 0);
    }

    Require(ftruncate_nid_postfix(fd, 10) == 0);
    Require(std::filesystem::file_size(path) == 10);

    char buffer10[10]{};
    Require(lseek_nid_postfix(fd, 0, SEEK_SET) == 0);
    Require(read_nid_postfix(fd, buffer10, 10) == 10);
    Require(std::memcmp(buffer10, "0123456789", 10) == 0);

    Require(sceKernelFtruncate(fd, 0) == 0);
    Require(std::filesystem::file_size(path) == 0);

    Require(ftruncate_nid_postfix(fd, 8) == 0);
    Require(std::filesystem::file_size(path) == 8);

    char buffer8[8]{};
    Require(lseek_nid_postfix(fd, 0, SEEK_SET) == 0);
    Require(read_nid_postfix(fd, buffer8, 8) == 8);
    for (std::size_t i = 0; i < 8; ++i) {
        Require(buffer8[i] == 0);
    }

    Require(lseek_nid_postfix(fd, 5, SEEK_SET) == 5);
    Require(ftruncate_nid_postfix(fd, 40) == 0);
    Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == 5);
    Require(ftruncate_nid_postfix(fd, 2) == 0);
    Require(lseek_nid_postfix(fd, 0, SEEK_CUR) == 5);

    Require(close_nid_postfix(fd) == 0);

    const int socketFd = socket_nid_postfix(2, 1, 0);
    if (socketFd >= 0) {
        RequireFailure(ftruncate_nid_postfix(socketFd, 0), GuestEinval);
        RequireFailure(ftruncate_nid_postfix(socketFd, 16), GuestEinval);
        Require(sceKernelFtruncate(socketFd, 0) == SceKernelErrorBase + GuestEinval);
        Require(close_nid_postfix(socketFd) == 0);
        RequireFailure(ftruncate_nid_postfix(socketFd, 0), GuestEbadf);
    }

    int pipeDescriptors[2]{-1, -1};
    if (pipe_nid_postfix(pipeDescriptors) == 0) {
        RequireFailure(ftruncate_nid_postfix(pipeDescriptors[0], 0), GuestEinval);
        RequireFailure(ftruncate_nid_postfix(pipeDescriptors[1], 0), GuestEinval);
        Require(sceKernelFtruncate(pipeDescriptors[0], 0) == SceKernelErrorBase + GuestEinval);
        Require(sceKernelFtruncate(pipeDescriptors[1], 0) == SceKernelErrorBase + GuestEinval);
        Require(close_nid_postfix(pipeDescriptors[0]) == 0);
        Require(close_nid_postfix(pipeDescriptors[1]) == 0);
    }

    return 0;
}
