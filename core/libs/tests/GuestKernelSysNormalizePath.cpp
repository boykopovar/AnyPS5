#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int GuestErrnoSentinel = 77;
constexpr char OutputSentinel = static_cast<char>(0x5A);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "normalize path check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

std::size_t HostPageSize() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return static_cast<std::size_t>(info.dwPageSize);
#else
    const long size = ::sysconf(_SC_PAGESIZE);
    Require(size > 0);
    return static_cast<std::size_t>(size);
#endif
}

struct GuardedPages {
    std::uint8_t* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() : pageSize(HostPageSize()) {
#ifdef _WIN32
        base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, pageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &previous) != FALSE);
#else
        void* mapped = ::mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<std::uint8_t*>(mapped);
        Require(::mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        ::munmap(base, pageSize * 2);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;
};

struct ReadOnlyPage {
    std::uint8_t* base = nullptr;
    std::size_t pageSize = 0;

    ReadOnlyPage() : pageSize(HostPageSize()) {
#ifdef _WIN32
        base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(base != nullptr);
        std::memset(base, static_cast<unsigned char>(OutputSentinel), pageSize);
        DWORD previous = 0;
        Require(VirtualProtect(base, pageSize, PAGE_READONLY, &previous) != FALSE);
#else
        void* mapped = ::mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<std::uint8_t*>(mapped);
        std::memset(base, static_cast<unsigned char>(OutputSentinel), pageSize);
        Require(::mprotect(base, pageSize, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        ::munmap(base, pageSize);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;
};

void ExpectSuccess(std::string_view input, std::string_view expected, std::source_location location = std::source_location::current()) {
    std::string buffer(input);
    std::vector<char> output(expected.size() + 8, OutputSentinel);
    *__error_nid_postfix() = GuestErrnoSentinel;
    const int result = sceKernelNormalizePath(buffer.c_str(), buffer.size() + 1, output.data(), output.size());
    Require(result == 0, location);
    Require(*__error_nid_postfix() == GuestErrnoSentinel, location);
    Require(std::string_view(output.data()) == expected, location);
    for (std::size_t i = expected.size() + 1; i < output.size(); ++i) {
        Require(output[i] == OutputSentinel, location);
    }

    std::vector<char> exact(expected.size() + 1, OutputSentinel);
    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath(buffer.c_str(), buffer.size() + 1, exact.data(), exact.size()) == 0, location);
    Require(*__error_nid_postfix() == GuestErrnoSentinel, location);
    Require(std::string_view(exact.data()) == expected, location);

    std::vector<char> tooSmall(expected.size() + 4, OutputSentinel);
    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath(buffer.c_str(), buffer.size() + 1, tooSmall.data(), expected.size()) == SCE_KERNEL_ERROR_EINVAL, location);
    Require(*__error_nid_postfix() == GuestErrnoSentinel, location);
    for (const char byte : tooSmall) Require(byte == OutputSentinel, location);
}

void ExpectError(const char* path, std::size_t pathSize, int expectedError, std::size_t outputSize = 64, std::source_location location = std::source_location::current()) {
    std::vector<char> output(std::max<std::size_t>(outputSize, 16), OutputSentinel);
    *__error_nid_postfix() = GuestErrnoSentinel;
    const int result = sceKernelNormalizePath(path, pathSize, output.data(), outputSize);
    Require(result == expectedError, location);
    Require(*__error_nid_postfix() == GuestErrnoSentinel, location);
    for (const char byte : output) Require(byte == OutputSentinel, location);
}

void TestNormalizationCases() {
    ExpectSuccess("/", "/");
    ExpectSuccess("//", "/");
    ExpectSuccess("///", "/");
    ExpectSuccess("/.", "/");
    ExpectSuccess("/./", "/");
    ExpectSuccess("/./.", "/");
    ExpectSuccess("/././", "/");
    ExpectSuccess("/..", "/");
    ExpectSuccess("/../", "/");
    ExpectSuccess("/../..", "/");
    ExpectSuccess("/../../", "/");
    ExpectSuccess("/../a", "/a");
    ExpectSuccess("/../a/", "/a/");
    ExpectSuccess("/../../../a/b", "/a/b");
    ExpectSuccess("/a", "/a");
    ExpectSuccess("/a/", "/a/");
    ExpectSuccess("/a//", "/a/");
    ExpectSuccess("/a///", "/a/");
    ExpectSuccess("//a//b", "/a/b");
    ExpectSuccess("//a//b//", "/a/b/");
    ExpectSuccess("/a/.", "/a");
    ExpectSuccess("/a/./", "/a/");
    ExpectSuccess("/a/./b", "/a/b");
    ExpectSuccess("/a/./b/", "/a/b/");
    ExpectSuccess("/a/..", "/");
    ExpectSuccess("/a/../", "/");
    ExpectSuccess("/a/..//", "/");
    ExpectSuccess("/a/b/..", "/a");
    ExpectSuccess("/a/b/../", "/a/");
    ExpectSuccess("/a/b/../c", "/a/c");
    ExpectSuccess("/a/b/../c/", "/a/c/");
    ExpectSuccess("/a/b/../../c", "/c");
    ExpectSuccess("/a/b/../../../c", "/c");
    ExpectSuccess("/a//..", "/");
    ExpectSuccess("/a//../", "/");
    ExpectSuccess("/a/./..", "/");
    ExpectSuccess("/a/./../", "/");
    ExpectSuccess("/a/...", "/a/...");
    ExpectSuccess("/a/..../", "/a/..../");
    ExpectSuccess("/a/.b", "/a/.b");
    ExpectSuccess("/a/b.", "/a/b.");
    ExpectSuccess("/a/..b", "/a/..b");
    ExpectSuccess("/a/b..", "/a/b..");

    const std::string withEmbeddedNul("/a/b\0/../../ignored", 19);
    std::array<char, 16> out{};
    out.fill(OutputSentinel);
    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath(withEmbeddedNul.data(), withEmbeddedNul.size(), out.data(), out.size()) == 0);
    Require(*__error_nid_postfix() == GuestErrnoSentinel);
    Require(std::string_view(out.data()) == "/a/b");
}

void TestLengthLimits() {
    const std::string maxComponent = "/" + std::string(1022, 'a');
    ExpectSuccess(maxComponent, maxComponent);

    const std::string overComponent = "/" + std::string(1023, 'a');
    ExpectError(overComponent.c_str(), overComponent.size() + 1, SCE_KERNEL_ERROR_ENAMETOOLONG, 2048);
    ExpectError(overComponent.c_str(), overComponent.size() + 1, SCE_KERNEL_ERROR_ENAMETOOLONG, 2);
    ExpectError(overComponent.c_str(), overComponent.size() + 1, SCE_KERNEL_ERROR_EINVAL, 1);

    const std::string maxWithTrailingSlash = "/" + std::string(1021, 'a') + "/";
    ExpectSuccess(maxWithTrailingSlash, maxWithTrailingSlash);

    const std::string overWithTrailingSlash = "/" + std::string(1022, 'a') + "/";
    ExpectError(overWithTrailingSlash.c_str(), overWithTrailingSlash.size() + 1, SCE_KERNEL_ERROR_ENAMETOOLONG, 2048);

    const std::string maxTwoParts = "/" + std::string(512, 'a') + "/" + std::string(509, 'b');
    ExpectSuccess(maxTwoParts, maxTwoParts);

    const std::string overTwoParts = "/" + std::string(512, 'a') + "/" + std::string(510, 'b');
    ExpectError(overTwoParts.c_str(), overTwoParts.size() + 1, SCE_KERNEL_ERROR_ENAMETOOLONG, 2048);

    const std::string maxFollowedByDot = "/" + std::string(1022, 'a') + "/.";
    ExpectSuccess(maxFollowedByDot, maxComponent);

    const std::string maxFollowedByDotDot = "/" + std::string(1022, 'a') + "/..";
    ExpectSuccess(maxFollowedByDotDot, "/");

    const std::string maxFollowedByDotDotSlash = "/" + std::string(1022, 'a') + "/../";
    ExpectSuccess(maxFollowedByDotDotSlash, "/");

    const std::string intermediateTooLong = "/" + std::string(2000, 'a') + "/..";
    ExpectError(intermediateTooLong.c_str(), intermediateTooLong.size() + 1, SCE_KERNEL_ERROR_ENAMETOOLONG, 2048);

    const std::string intermediateBarelyOver = "/" + std::string(1022, 'a') + "/b/..";
    ExpectError(intermediateBarelyOver.c_str(), intermediateBarelyOver.size() + 1, SCE_KERNEL_ERROR_ENAMETOOLONG, 2048);

    std::string longCollapsible;
    for (int i = 0; i < 600; ++i) longCollapsible += "/ab/..";
    longCollapsible += "/end";
    ExpectSuccess(longCollapsible, "/end");
}

void TestArgumentValidationAndOverlap() {
    ExpectError(nullptr, 16, SCE_KERNEL_ERROR_EINVAL);
    ExpectError(nullptr, 0, SCE_KERNEL_ERROR_EINVAL);
    ExpectError(reinterpret_cast<const char*>(0x10), 0, SCE_KERNEL_ERROR_EINVAL);
    ExpectError("/a", 0, SCE_KERNEL_ERROR_EINVAL);
    ExpectError("/a", 2, SCE_KERNEL_ERROR_EINVAL);
    ExpectError("", 1, SCE_KERNEL_ERROR_EINVAL);
    ExpectError("a/b", 4, SCE_KERNEL_ERROR_EINVAL);
    ExpectError("./a", 4, SCE_KERNEL_ERROR_EINVAL);
    ExpectError("../a", 5, SCE_KERNEL_ERROR_EINVAL);
    ExpectError("/a", 3, SCE_KERNEL_ERROR_EINVAL, 0);
    ExpectError("/a", 3, SCE_KERNEL_ERROR_EINVAL, 1);
    ExpectError("/", 2, SCE_KERNEL_ERROR_EINVAL, 1);

    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath("/a", 3, nullptr, 16) == SCE_KERNEL_ERROR_EINVAL);
    Require(*__error_nid_postfix() == GuestErrnoSentinel);

    std::array<char, 64> region{};
    region.fill(OutputSentinel);
    std::memcpy(region.data(), "/a/b", 5);
    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath(region.data(), 5, region.data(), 10) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNormalizePath(region.data(), 5, region.data() + 4, 10) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNormalizePath(region.data(), 6, region.data() + 5, 10) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNormalizePath(region.data(), 10, region.data() + 8, 10) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNormalizePath(region.data(), 5, region.data() + 5, 10) == 0);
    Require(std::string_view(region.data() + 5) == "/a/b");
    Require(*__error_nid_postfix() == GuestErrnoSentinel);

    region.fill(OutputSentinel);
    std::memcpy(region.data() + 10, "/a/b", 5);
    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath(region.data() + 10, 5, region.data(), 11) == SCE_KERNEL_ERROR_EINVAL);
    for (std::size_t i = 0; i < 10; ++i) Require(region[i] == OutputSentinel);
    Require(sceKernelNormalizePath(region.data() + 10, 5, region.data(), 10) == 0);
    Require(std::string_view(region.data()) == "/a/b");
    Require(*__error_nid_postfix() == GuestErrnoSentinel);

    constexpr auto maxAddress = std::numeric_limits<std::uintptr_t>::max();
    std::array<char, 32> pair{};
    pair.fill(OutputSentinel);
    std::memcpy(pair.data() + 16, "/a/.", 5);
    const char* highInput = pair.data() + 16;
    char* lowOutput = pair.data();
    const auto highAddress = reinterpret_cast<std::uintptr_t>(highInput);
    const auto lowAddress = reinterpret_cast<std::uintptr_t>(lowOutput);

    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath(highInput, maxAddress - highAddress + 1, lowOutput, 16) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelNormalizePath(highInput, 5, lowOutput, maxAddress - lowAddress + 1) == SCE_KERNEL_ERROR_EINVAL);
    for (std::size_t i = 0; i < 16; ++i) Require(lowOutput[i] == OutputSentinel);
    Require(sceKernelNormalizePath(highInput, maxAddress - highAddress, lowOutput, 16) == 0);
    Require(std::string_view(lowOutput) == "/a");
    Require(*__error_nid_postfix() == GuestErrnoSentinel);
}

void TestGuestMemorySafety() {
    std::array<char, 32> output{};
    output.fill(OutputSentinel);

    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath(reinterpret_cast<const char*>(0x10), 16, output.data(), output.size()) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == GuestErrnoSentinel);
    for (const char byte : output) Require(byte == OutputSentinel);

    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath(reinterpret_cast<const char*>(0x10), 16, nullptr, 0) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == GuestErrnoSentinel);

    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath("/a", 3, reinterpret_cast<char*>(0x10), 16) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == GuestErrnoSentinel);

    GuardedPages guarded;
    char* nearEnd = reinterpret_cast<char*>(guarded.base + guarded.pageSize - 4);
    std::memcpy(nearEnd, "/a/", 4);
    output.fill(OutputSentinel);
    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath(nearEnd, 4096, output.data(), output.size()) == 0);
    Require(*__error_nid_postfix() == GuestErrnoSentinel);
    Require(std::string_view(output.data()) == "/a/");

    char* unterminatedNearEnd = reinterpret_cast<char*>(guarded.base + guarded.pageSize - 3);
    std::memcpy(unterminatedNearEnd, "/ab", 3);
    output.fill(OutputSentinel);
    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath(unterminatedNearEnd, 3, output.data(), output.size()) == SCE_KERNEL_ERROR_EINVAL);
    Require(*__error_nid_postfix() == GuestErrnoSentinel);
    for (const char byte : output) Require(byte == OutputSentinel);

    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath(unterminatedNearEnd, 4, output.data(), output.size()) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == GuestErrnoSentinel);
    for (const char byte : output) Require(byte == OutputSentinel);

    ReadOnlyPage readOnly;
    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath("/a", 3, reinterpret_cast<char*>(readOnly.base), 16) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == GuestErrnoSentinel);

    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath("relative", 9, reinterpret_cast<char*>(readOnly.base), 16) == SCE_KERNEL_ERROR_EINVAL);
    Require(*__error_nid_postfix() == GuestErrnoSentinel);

    char* outputAcrossGuard = reinterpret_cast<char*>(guarded.base + guarded.pageSize - 2);
    *__error_nid_postfix() = GuestErrnoSentinel;
    Require(sceKernelNormalizePath("/abc", 5, outputAcrossGuard, 16) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == GuestErrnoSentinel);
}

}

int main() {
    TestNormalizationCases();
    TestLengthLimits();
    TestArgumentValidationAndOverlap();
    TestGuestMemorySafety();
    std::puts("guest_kernel_sys_normalize_path passed");
    return 0;
}
