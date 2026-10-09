#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#if defined(__linux__)
#include <fstream>
#include <string>
#endif

extern "C" {
int APS5_VABI sceKernelMapFlexibleMemory(void**, std::size_t, int, int);
int APS5_VABI sceKernelReserveVirtualRange(void**, std::size_t, int, std::size_t);
int APS5_VABI sceKernelMunmap(void*, std::size_t);
int APS5_VABI mlock_nid_postfix(const void*, std::size_t);
int APS5_VABI munlock_nid_postfix(const void*, std::size_t);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool value) { if (!value) std::abort(); }

#if defined(__linux__)
static std::size_t LockedKilobytes() {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmLck:", 0) == 0) return std::strtoull(line.c_str() + 6, nullptr, 10);
    }
    return 0;
}
#endif

int main() {
    constexpr int untouched = 12345;
    constexpr std::size_t page = 0x4000;
#ifdef _WIN32
    constexpr std::size_t length = 0x400000;
#else
    constexpr std::size_t length = 0x10000;
#endif
    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, length, 3, 0) == 0);
    auto* bytes = static_cast<unsigned char*>(mapped);

    *__error_nid_postfix() = untouched;
    Require(mlock_nid_postfix(mapped, 0) == 0);
    Require(*__error_nid_postfix() == untouched);
#if defined(__linux__)
    const auto lockedBefore = LockedKilobytes();
#endif
    Require(mlock_nid_postfix(bytes + 1, length - page) == 0);
#if defined(__linux__)
    Require(LockedKilobytes() - lockedBefore == length / 1024);
#endif
    Require(mlock_nid_postfix(mapped, length) == 0);
    Require(mlock_nid_postfix(mapped, length) == 0);
    bytes[length - 1] = 7;

    *__error_nid_postfix() = untouched;
    Require(munlock_nid_postfix(mapped, 0) == 0);
    Require(*__error_nid_postfix() == untouched);
    Require(munlock_nid_postfix(bytes + 1, length - page) == 0);
#if defined(__linux__)
    Require(LockedKilobytes() == lockedBefore);
#elif defined(_WIN32)
    Require(!VirtualUnlock(mapped, length) && GetLastError() == ERROR_NOT_LOCKED);
#endif
    *__error_nid_postfix() = untouched;
    Require(munlock_nid_postfix(mapped, length) == 0);
    Require(*__error_nid_postfix() == untouched);
    bytes[0] = 7;
    Require(mlock_nid_postfix(mapped, page) == 0);
#if defined(__linux__)
    Require(LockedKilobytes() - lockedBefore == page / 1024);
#endif
    Require(munlock_nid_postfix(mapped, length) == 0);
#if defined(__linux__)
    Require(LockedKilobytes() == lockedBefore);
#elif defined(_WIN32)
    Require(!VirtualUnlock(mapped, page) && GetLastError() == ERROR_NOT_LOCKED);
#endif
    Require(mlock_nid_postfix(mapped, page) == 0);
    Require(munlock_nid_postfix(mapped, page) == 0);
#if defined(__linux__)
    Require(LockedKilobytes() == lockedBefore);
#elif defined(_WIN32)
    Require(!VirtualUnlock(mapped, page) && GetLastError() == ERROR_NOT_LOCKED);
#endif

    *__error_nid_postfix() = 0;
    Require(mlock_nid_postfix(reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - page + 1), page * 2) == -1);
    Require(*__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(munlock_nid_postfix(reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - page + 1), page * 2) == -1);
    Require(*__error_nid_postfix() == 22);

    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page, 0, 0) == 0);
    *__error_nid_postfix() = 0;
    Require(mlock_nid_postfix(reserved, page) == -1 && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = untouched;
    Require(munlock_nid_postfix(reserved, page) == 0);
    Require(*__error_nid_postfix() == untouched);
    Require(sceKernelMunmap(reserved, page) == 0);

    Require(sceKernelMunmap(mapped, length) == 0);
    *__error_nid_postfix() = 0;
    Require(mlock_nid_postfix(mapped, page) == -1 && *__error_nid_postfix() == 12);
    *__error_nid_postfix() = 0;
    Require(munlock_nid_postfix(mapped, page) == -1 && *__error_nid_postfix() == 12);
    return 0;
}
