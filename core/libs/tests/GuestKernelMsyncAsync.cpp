#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>

namespace {
constexpr std::size_t Page = 0x4000;
constexpr int ReadWrite = 3;
constexpr int PrivateAnonymous = 0x1002;
constexpr int Sync = 0x0;
constexpr int Async = 0x1;
constexpr int Invalidate = 0x2;
constexpr int Invalid = 22;
constexpr int NoMemory = 12;
constexpr int Busy = 16;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "msync async bounds check failed at line %u\n", static_cast<unsigned>(location.line()));
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

char* Map(std::size_t pages) {
    void* mapped = mmap_nid_postfix(nullptr, pages * Page, ReadWrite, PrivateAnonymous, -1, 0);
    Require(mapped != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    return static_cast<char*>(mapped);
}
} // namespace

int main() {
    char* mapped = Map(4);
    mapped[0] = 42;

    Require(Succeeds(mapped, Page, Async));
    Require(Succeeds(mapped, 2 * Page, Async));
    Require(Fails(mapped, Page, Async | Invalidate, Invalid));

    Require(Fails(mapped, Page, Invalidate, Busy));
    Require(Fails(mapped, 2 * Page, Sync | Invalidate, Busy));

    Require(Succeeds(mapped + Page, Page, Sync));
    Require(Succeeds(mapped + Page, Page, Async));

    Require(munmap_nid_postfix(mapped, 4 * Page) == 0);
    Require(Fails(mapped, Page, Async, NoMemory));
    return 0;
}
