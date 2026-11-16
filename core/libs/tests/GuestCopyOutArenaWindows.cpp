#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <windows.h>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "guest copy-out arena check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    constexpr std::size_t bytes = 65536;
    constexpr std::size_t page = 4096;
    constexpr std::size_t middle = 5 * page;
    constexpr std::size_t readOnly = 9 * page;
    auto* arena = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, bytes));
    Require(arena != nullptr);
    GuestArena::GuestArenaCommit_nid_postfix(arena, bytes, PAGE_READWRITE, bytes);
    std::vector<unsigned char> source(bytes);
    for (std::size_t index = 0; index < bytes; ++index) source[index] = static_cast<unsigned char>(index * 31 >> 3);
    errno = 71;
    Require(GuestCopyOut_nid_no_patch(arena, source.data(), bytes) && std::memcmp(arena, source.data(), bytes) == 0);
    for (std::size_t length = 1; length < 80; ++length) {
        std::memset(arena + middle - 100, 0, 100);
        Require(GuestCopyOut_nid_no_patch(arena + middle - length, source.data(), length));
        Require(std::memcmp(arena + middle - length, source.data(), length) == 0);
        Require(arena[middle - length - 1] == 0);
    }
    Require(VirtualFree(arena + middle, page, MEM_DECOMMIT));
    Require(!GuestCopyOut_nid_no_patch(arena + middle - 8, source.data(), 16));
    Require(!GuestCopyOut_nid_no_patch(arena + middle, source.data(), 1));
    Require(GuestCopyOut_nid_no_patch(arena + middle + page, source.data(), page));
    Require(std::memcmp(arena + middle + page, source.data(), page) == 0);
    DWORD previous = 0;
    Require(VirtualProtect(arena + readOnly, page, PAGE_READONLY, &previous));
    Require(!GuestCopyOut_nid_no_patch(arena + readOnly - 8, source.data(), 16));
    Require(!GuestCopyOut_nid_no_patch(arena + readOnly, source.data(), 8));
    Require(errno == 71);
    auto* host = static_cast<unsigned char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(host != nullptr);
    Require(VirtualProtect(host + page, page, PAGE_READONLY, &previous));
    Require(GuestCopyOut_nid_no_patch(host, source.data(), page) && std::memcmp(host, source.data(), page) == 0);
    Require(!GuestCopyOut_nid_no_patch(host + page - 8, source.data(), 16));
    Require(!GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(0x10), source.data(), 8));
    Require(errno == 71);
    std::puts("guest copy-out arena tests passed");
    return 0;
}
