#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "guest copy hook errno check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static int g_reads = 0;
static int g_writes = 0;

static void ClobberRead(std::uintptr_t, std::size_t) {
    ++g_reads;
    errno = 99;
}

static void ClobberWrite(std::uintptr_t, std::size_t) {
    ++g_writes;
    errno = 99;
}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberRead);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberWrite);
    void* const unmapped = reinterpret_cast<void*>(0x10);
    const std::uint64_t source = 0x1122334455667788ull;
    std::uint64_t destination = 0;
    errno = 71;
    Require(GuestCopyIn_nid_no_patch(&destination, &source, sizeof(source)) && destination == source && errno == 71);
    destination = 0;
    Require(GuestCopyOut_nid_no_patch(&destination, &source, sizeof(source)) && destination == source && errno == 71);
    Require(!GuestCopyIn_nid_no_patch(&destination, unmapped, sizeof(destination)) && errno == 71);
    Require(!GuestCopyOut_nid_no_patch(unmapped, &source, sizeof(source)) && errno == 71);
    Require(g_reads == 2 && g_writes == 2);
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    std::puts("guest copy hook errno tests passed");
    return 0;
}
