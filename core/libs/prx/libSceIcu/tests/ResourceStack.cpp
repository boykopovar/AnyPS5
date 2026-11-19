#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
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

static_assert(std::is_same_v<decltype(&ures_initStackObject_nid_postfix), void (APS5_VABI*)(UResourceBundle*)>);

struct GuardedBundle {
    std::array<std::uint8_t, 32> before;
    std::array<std::uint8_t, 200> bundle;
    std::array<std::uint8_t, 32> after;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU resource stack: %s\n", message);
    std::abort();
}

void RequireGuards(const GuardedBundle& bundle, std::uint8_t value) {
    for (const auto byte : bundle.before) Require(byte == value, "ures_initStackObject preserves the leading guard");
    for (const auto byte : bundle.after) Require(byte == value, "ures_initStackObject preserves the trailing guard");
}

void RequireRejected(void* bundle) {
    errno = 73;
    bool rejected = false;
    try {
        ures_initStackObject_nid_postfix(static_cast<UResourceBundle*>(bundle));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "ures_initStackObject rejects an unwritable bundle");
    Require(errno == 73, "ures_initStackObject preserves errno for an unwritable bundle");
}

void UnwritableBundles() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    auto* pages = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 3 * pageSize, MEM_RESERVE, PAGE_NOACCESS));
    Require(pages != nullptr && VirtualAlloc(pages, 2 * pageSize, MEM_COMMIT, PAGE_READWRITE) != nullptr, "guard pages");
#else
    const auto pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* mapping = mmap(nullptr, 3 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "guard pages");
    auto* pages = static_cast<std::uint8_t*>(mapping);
    Require(mprotect(pages + 2 * pageSize, pageSize, PROT_NONE) == 0, "guard page");
#endif
    std::memset(pages, 0x5a, 2 * pageSize);
#ifdef _WIN32
    DWORD old = 0;
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_READONLY, &old) != 0, "read-only page");
#else
    Require(mprotect(pages + pageSize, pageSize, PROT_READ) == 0, "read-only page");
#endif
    RequireRejected(nullptr);
    RequireRejected(pages + 2 * pageSize);
    RequireRejected(pages + pageSize);
    RequireRejected(pages + pageSize - 100);
    for (std::size_t index = 0; index < 2 * pageSize; ++index) Require(pages[index] == 0x5a, "ures_initStackObject leaves an unwritable bundle untouched");
    ures_initStackObject_nid_postfix(reinterpret_cast<UResourceBundle*>(pages + pageSize - 200));
    for (std::size_t index = pageSize - 200; index < pageSize; ++index) Require(pages[index] == 0, "ures_initStackObject clears a bundle that ends at a page end");
    Require(pages[pageSize - 201] == 0x5a, "ures_initStackObject preserves the byte before the bundle");
}

}

int main() {
    UnwritableBundles();
    GuardedBundle actual;
    for (const std::uint8_t value : {std::uint8_t{0}, std::uint8_t{0x5a}, std::uint8_t{0xff}}) {
        actual.before.fill(value);
        actual.bundle.fill(value);
        actual.after.fill(value);
        errno = 73;
        ures_initStackObject_nid_postfix(reinterpret_cast<UResourceBundle*>(actual.bundle.data()));
        Require(errno == 73, "ures_initStackObject preserves errno");
        RequireGuards(actual, value);
        for (const auto byte : actual.bundle) Require(byte == 0, "ures_initStackObject clears the complete ICU 68 bundle");
    }
}
