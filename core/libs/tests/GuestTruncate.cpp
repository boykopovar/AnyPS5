#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
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

extern "C" {
int APS5_VABI truncate_nid_postfix(const char*, std::int64_t);
int APS5_VABI sceKernelTruncate_nid_postfix(const char*, std::int64_t);
int APS5_VABI ftruncate_nid_postfix(int, std::int64_t);
int APS5_VABI sceKernelFtruncate(int, std::int64_t);
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t);
int APS5_VABI sceKernelClose(int);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

constexpr int GuestEnoent = 2;
constexpr int GuestEbadf = 9;
constexpr int GuestEacces = 13;
constexpr int GuestEfault = 14;
constexpr int GuestEisdir = 21;
constexpr int GuestEinval = 22;
constexpr int SceKernelErrorBase = static_cast<int>(0x80020000u);

static void RequireFailure(int result, int error) {
    Require(result == -1);
    Require(*__error_nid_postfix() == error);
}

static std::string Contents(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

#ifdef _WIN32
static unsigned parameterReports = 0;

static void CountParameterReport(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {
    ++parameterReports;
}

static void CheckDescriptorFailures(const char* path) {
    const int closed = sceKernelOpen(path, 2, 0);
    Require(closed >= 0 && sceKernelClose(closed) == 0);
    const auto previous = _set_thread_local_invalid_parameter_handler(CountParameterReport);
    for (const int descriptor : {-2, -1, closed, std::numeric_limits<int>::max()}) {
        for (const std::int64_t length : {std::int64_t{0}, std::int64_t{8}}) {
            RequireFailure(ftruncate_nid_postfix(descriptor, length), GuestEbadf);
            Require(sceKernelFtruncate(descriptor, length) == SceKernelErrorBase + GuestEbadf);
        }
    }
    Require(_set_thread_local_invalid_parameter_handler(previous) == CountParameterReport);
    if (parameterReports != 0) std::fprintf(stderr, "ftruncate invoked the CRT parameter handler %u times\n", parameterReports);
    Require(parameterReports == 0);
    RequireFailure(ftruncate_nid_postfix(closed, 0), GuestEbadf);
    Require(sceKernelFtruncate(-1, 0) == SceKernelErrorBase + GuestEbadf);
}
#endif

int main() {
    const auto name = "anyps5-truncate-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto directory = std::filesystem::path(name);
    Require(std::filesystem::create_directory(directory));
    const auto file = directory / "sized.txt";
    { std::ofstream output(file, std::ios::binary); output << "0123456789abcdef"; }
    const auto path = file.string();
#ifdef _WIN32
    CheckDescriptorFailures(path.c_str());
    Require(Contents(file) == "0123456789abcdef");
#endif

    Require(truncate_nid_postfix(path.c_str(), 6) == 0);
    Require(std::filesystem::file_size(file) == 6);
    Require(Contents(file) == "012345");

    Require(truncate_nid_postfix(path.c_str(), 9) == 0);
    Require(std::filesystem::file_size(file) == 9);
    Require(Contents(file) == std::string("012345\0\0\0", 9));

    Require(truncate_nid_postfix(path.c_str(), 0) == 0);
    Require(std::filesystem::file_size(file) == 0);

    RequireFailure(truncate_nid_postfix(path.c_str(), -1), GuestEinval);
    Require(std::filesystem::file_size(file) == 0);
    RequireFailure(truncate_nid_postfix((directory / "missing").string().c_str(), 0), GuestEnoent);
    RequireFailure(truncate_nid_postfix("", 0), GuestEnoent);
    RequireFailure(truncate_nid_postfix(nullptr, 0), GuestEfault);
    RequireFailure(truncate_nid_postfix(directory.string().c_str(), 0), GuestEisdir);
    Require(std::filesystem::is_directory(directory));
    Require(sceKernelTruncate_nid_postfix(directory.string().c_str(), 0) == SceKernelErrorBase + GuestEisdir);
    Require(sceKernelTruncate_nid_postfix(path.c_str(), -1) == SceKernelErrorBase + GuestEinval);

    { std::ofstream output(file, std::ios::binary); output << "locked"; }
#ifdef _WIN32
    Require(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_READONLY));
    RequireFailure(truncate_nid_postfix(path.c_str(), 1), GuestEacces);
    Require(sceKernelTruncate_nid_postfix(path.c_str(), 1) == SceKernelErrorBase + GuestEacces);
    Require(SetFileAttributesW(file.c_str(), FILE_ATTRIBUTE_NORMAL));
#else
    std::filesystem::permissions(file, std::filesystem::perms::owner_read);
    if (::geteuid() != 0) {
        RequireFailure(truncate_nid_postfix(path.c_str(), 1), GuestEacces);
        Require(sceKernelTruncate_nid_postfix(path.c_str(), 1) == SceKernelErrorBase + GuestEacces);
    } else {
        std::fprintf(stderr, "running as root, which ignores file permissions: the read-only cases not tested\n");
    }
    std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
#endif
    Require(Contents(file) == "locked");

    Require(std::filesystem::remove(file));
    Require(std::filesystem::remove(directory));
}
