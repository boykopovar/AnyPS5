#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

extern "C" {
void* APS5_VABI mmap_nid_postfix(void*, std::size_t, int, int, int, std::int64_t) noexcept;
int APS5_VABI munmap_nid_postfix(void*, std::size_t) noexcept;
int APS5_VABI msync_nid_postfix(void*, std::size_t, int);
int APS5_VABI sceKernelReserveVirtualRange(void**, std::size_t, int, std::size_t);
int* APS5_VABI __error_nid_postfix();
}

namespace {
constexpr std::size_t Page = 0x4000;
constexpr int ReadWrite = 3;
constexpr int PrivateAnonymous = 0x1002;
constexpr int Sync = 0x0;
constexpr int Async = 0x1;
constexpr int Invalidate = 0x2;
constexpr int Invalid = 22;
constexpr int NoMemory = 12;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "msync check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

bool Succeeds(void* address, std::size_t length, int flags) {
    *__error_nid_postfix() = 77;
    return msync_nid_postfix(address, length, flags) == 0 && *__error_nid_postfix() == 77;
}

bool Fails(void* address, std::size_t length, int flags, int error) {
    *__error_nid_postfix() = 0;
    return msync_nid_postfix(address, length, flags) == -1 && *__error_nid_postfix() == error;
}

bool Throws(void* address, std::size_t length, int flags) {
    try {
        msync_nid_postfix(address, length, flags);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

char* Map(std::size_t pages) {
    void* mapped = mmap_nid_postfix(nullptr, pages * Page, ReadWrite, PrivateAnonymous, -1, 0);
    Require(mapped != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    return static_cast<char*>(mapped);
}
}

int main() {
    char* mapped = Map(3);
    mapped[0] = 1;
    Require(Succeeds(mapped, 3 * Page, Sync));
    Require(Succeeds(mapped, 3 * Page, Async));
    Require(Succeeds(mapped + 100, 10, Sync));
    Require(Succeeds(mapped + Page - 1, 2, Sync));
    Require(Succeeds(mapped + Page, 0, Sync));
    Require(Succeeds(mapped, Page, 0x4));

    Require(Fails(mapped, Page, Async | Invalidate, Invalid));
    const auto top = std::numeric_limits<std::uintptr_t>::max() & ~(Page - 1);
    Require(Fails(reinterpret_cast<void*>(top), 2 * Page, Sync, Invalid));
    Require(Throws(mapped, Page, Invalidate));

    Require(munmap_nid_postfix(mapped, 3 * Page) == 0);
    Require(Fails(mapped, Page, Sync, NoMemory));
    Require(Fails(mapped + Page, 3 * Page, Async, NoMemory));
    Require(Fails(mapped + Page, 0, Sync, NoMemory));
    Require(Fails(mapped, Page, Invalidate, NoMemory));

    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, 2 * Page, 0, 0) == 0);
    Require(Succeeds(reserved, 2 * Page, Sync));
    Require(Succeeds(reserved, 0, Async));
    return 0;
}
