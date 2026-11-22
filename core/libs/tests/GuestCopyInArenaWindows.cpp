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
        std::fprintf(stderr, "guest copy-in arena check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    constexpr std::size_t bytes = 65536;
    constexpr std::size_t page = 4096;
    constexpr std::size_t middle = 5 * page;
    auto* arena = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, bytes));
    Require(arena != nullptr);
    GuestArena::GuestArenaCommit_nid_postfix(arena, bytes, PAGE_READWRITE, bytes);
    for (std::size_t index = 0; index < bytes; ++index) arena[index] = static_cast<unsigned char>(index * 31 >> 3);
    std::vector<unsigned char> copy(bytes);
    errno = 71;
    Require(GuestCopyIn_nid_no_patch(copy.data(), arena, bytes) && std::memcmp(copy.data(), arena, bytes) == 0);
    for (std::size_t length = 1; length < 80; ++length) {
        std::vector<unsigned char> edge(length, 0xff);
        Require(GuestCopyIn_nid_no_patch(edge.data(), arena + middle - length, length));
        Require(std::memcmp(edge.data(), arena + middle - length, length) == 0);
    }
    Require(VirtualFree(arena + middle, page, MEM_DECOMMIT));
    Require(!GuestCopyIn_nid_no_patch(copy.data(), arena, bytes));
    Require(!GuestCopyIn_nid_no_patch(copy.data(), arena + middle - 8, 16));
    Require(!GuestCopyIn_nid_no_patch(copy.data(), arena + middle, 1));
    Require(GuestCopyIn_nid_no_patch(copy.data(), arena + middle + page, page));
    Require(std::memcmp(copy.data(), arena + middle + page, page) == 0);
    Require(errno == 71);
    auto* host = static_cast<unsigned char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(host != nullptr);
    std::memset(host, 0x5a, 2 * page);
    DWORD previous = 0;
    Require(VirtualProtect(host + page, page, PAGE_NOACCESS, &previous));
    Require(GuestCopyIn_nid_no_patch(copy.data(), host, page) && copy[0] == 0x5a && copy[page - 1] == 0x5a);
    Require(!GuestCopyIn_nid_no_patch(copy.data(), host + page - 8, 16));
    Require(!GuestCopyIn_nid_no_patch(copy.data(), reinterpret_cast<void*>(0x10), 8));
    Require(errno == 71);
    std::puts("guest copy-in arena tests passed");
    return 0;
}
