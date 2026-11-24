#include "prx/libSceIcu/libSceIcu.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <type_traits>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::int32_t UnsupportedError = 16;

static_assert(std::is_same_v<decltype(&uprv_dl_open_nid_postfix),
                             void* (APS5_VABI*)(const char*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&uprv_dl_close_nid_postfix),
                             void (APS5_VABI*)(void*, std::int32_t*)>);
static_assert(std::is_same_v<decltype(&uprv_dlsym_func_nid_postfix),
                             UVoidFunction* (APS5_VABI*)(void*, const char*, std::int32_t*)>);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU dynamic loading: %s\n", message);
    std::abort();
}

template <typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

void CheckUnsupportedStatus() {
    std::int32_t status = 0;
    Require(uprv_dl_open_nid_postfix("dummy.prx", &status) == nullptr, "dl_open returns null");
    Require(status == UnsupportedError, "dl_open reports unsupported error");

    status = 0;
    uprv_dl_close_nid_postfix(reinterpret_cast<void*>(std::uintptr_t{0x1000}), &status);
    Require(status == UnsupportedError, "dl_close reports unsupported error");

    status = 0;
    Require(uprv_dlsym_func_nid_postfix(reinterpret_cast<void*>(std::uintptr_t{0x1000}), "dummy_sym", &status) == nullptr,
            "dlsym_func returns null");
    Require(status == UnsupportedError, "dlsym_func reports unsupported error");

    status = -128;
    Require(uprv_dl_open_nid_postfix(nullptr, &status) == nullptr, "dl_open with warning status returns null");
    Require(status == UnsupportedError, "dl_open overwrites warning status with unsupported");

    status = -128;
    uprv_dl_close_nid_postfix(nullptr, &status);
    Require(status == UnsupportedError, "dl_close overwrites warning status with unsupported");

    status = -128;
    Require(uprv_dlsym_func_nid_postfix(nullptr, nullptr, &status) == nullptr, "dlsym_func with warning status returns null");
    Require(status == UnsupportedError, "dlsym_func overwrites warning status with unsupported");
}

void CheckPrefailedStatus() {
    for (const std::int32_t initial : std::initializer_list<std::int32_t>{1, 15, 16, 17, 100}) {
        std::int32_t status = initial;
        Require(uprv_dl_open_nid_postfix("dummy.prx", &status) == nullptr, "dl_open with prefailed returns null");
        Require(status == initial, "dl_open preserves prefailed status");

        status = initial;
        uprv_dl_close_nid_postfix(reinterpret_cast<void*>(std::uintptr_t{0x1000}), &status);
        Require(status == initial, "dl_close preserves prefailed status");

        status = initial;
        Require(uprv_dlsym_func_nid_postfix(reinterpret_cast<void*>(std::uintptr_t{0x1000}), "dummy_sym", &status) == nullptr,
                "dlsym_func with prefailed returns null");
        Require(status == initial, "dlsym_func preserves prefailed status");
    }
}

void CheckInvalidArguments() {
    RequireThrows([] { uprv_dl_open_nid_postfix("dummy.prx", nullptr); }, "dl_open null status throws");
    RequireThrows([] { uprv_dl_close_nid_postfix(nullptr, nullptr); }, "dl_close null status throws");
    RequireThrows([] { uprv_dlsym_func_nid_postfix(nullptr, "sym", nullptr); }, "dlsym_func null status throws");

    auto* unmapped = reinterpret_cast<std::int32_t*>(std::uintptr_t{0x10});
    errno = 42;
    RequireThrows([&] { uprv_dl_open_nid_postfix("dummy.prx", unmapped); }, "dl_open unmapped status throws");
    Require(errno == 42, "dl_open unmapped status preserves errno");

    errno = 42;
    RequireThrows([&] { uprv_dl_close_nid_postfix(nullptr, unmapped); }, "dl_close unmapped status throws");
    Require(errno == 42, "dl_close unmapped status preserves errno");

    errno = 42;
    RequireThrows([&] { uprv_dlsym_func_nid_postfix(nullptr, "sym", unmapped); }, "dlsym_func unmapped status throws");
    Require(errno == 42, "dlsym_func unmapped status preserves errno");
}

void CheckReadOnlyStatus() {
    const std::size_t pageSize =
#ifdef _WIN32
        4096;
#else
        static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
#endif
    void* roPage = nullptr;
#ifdef _WIN32
    roPage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(roPage != nullptr, "allocate page");
#else
    roPage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(roPage != MAP_FAILED, "mmap page");
#endif
    const std::int32_t zero = 0;
    const std::int32_t prefailed = 7;
    std::memcpy(roPage, &zero, sizeof(zero));
    std::memcpy(static_cast<char*>(roPage) + sizeof(std::int32_t), &prefailed, sizeof(prefailed));
#ifdef _WIN32
    DWORD oldProtect = 0;
    Require(VirtualProtect(roPage, pageSize, PAGE_READONLY, &oldProtect) != 0, "protect read-only");
#else
    Require(mprotect(roPage, pageSize, PROT_READ) == 0, "mprotect read-only");
#endif
    auto* roZero = static_cast<std::int32_t*>(roPage);
    auto* roPrefailed = reinterpret_cast<std::int32_t*>(static_cast<char*>(roPage) + sizeof(std::int32_t));

    RequireThrows([&] { uprv_dl_open_nid_postfix("dummy.prx", roZero); }, "dl_open read-only status throws");
    RequireThrows([&] { uprv_dl_close_nid_postfix(nullptr, roZero); }, "dl_close read-only status throws");
    RequireThrows([&] { uprv_dlsym_func_nid_postfix(nullptr, "sym", roZero); }, "dlsym_func read-only status throws");
    Require(*roZero == 0, "read-only status unmodified on throws");

    Require(uprv_dl_open_nid_postfix("dummy.prx", roPrefailed) == nullptr, "dl_open prefailed read-only returns null");
    uprv_dl_close_nid_postfix(nullptr, roPrefailed);
    Require(uprv_dlsym_func_nid_postfix(nullptr, "sym", roPrefailed) == nullptr, "dlsym_func prefailed read-only returns null");
    Require(*roPrefailed == prefailed, "prefailed status unmodified");

#ifdef _WIN32
    VirtualFree(roPage, 0, MEM_RELEASE);
#else
    munmap(roPage, pageSize);
#endif
}

}

int main() {
    CheckUnsupportedStatus();
    CheckPrefailedStatus();
    CheckInvalidArguments();
    CheckReadOnlyStatus();
    return 0;
}
