#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestStacks.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "guest copy stack check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void CheckCurrentStack() {
    const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    auto* const low = static_cast<unsigned char*>(tib->StackLimit);
    auto* const high = static_cast<unsigned char*>(tib->StackBase);
    unsigned char local[96];
    unsigned char copy[96];
    unsigned char source[96];
    for (std::size_t index = 0; index < sizeof(local); ++index) {
        local[index] = static_cast<unsigned char>(index * 13 + 5);
        source[index] = static_cast<unsigned char>(index * 7 + 3);
    }
    for (std::size_t length = 1; length <= sizeof(local); ++length) {
        std::memset(copy, 0, sizeof(copy));
        Require(GuestCopyIn_nid_no_patch(copy, local + sizeof(local) - length, length));
        Require(std::memcmp(copy, local + sizeof(local) - length, length) == 0);
    }
    for (std::size_t length = 1; length < sizeof(local); ++length) {
        std::memset(local, 0, sizeof(local));
        Require(GuestCopyOut_nid_no_patch(local + 1, source, length));
        Require(local[0] == 0 && std::memcmp(local + 1, source, length) == 0 && (length + 1 == sizeof(local) || local[length + 1] == 0));
        Require(GuestWritable_nid_no_patch(local + 1, length));
    }
    Require(GuestCopyIn_nid_no_patch(copy, low, 16) && std::memcmp(copy, low, 16) == 0);
    Require(GuestCopyIn_nid_no_patch(copy, high - 16, 16) && std::memcmp(copy, high - 16, 16) == 0);
    Require(GuestCopyOut_nid_no_patch(high - 16, copy, 16) && std::memcmp(copy, high - 16, 16) == 0);
    Require(GuestWritable_nid_no_patch(low, static_cast<std::size_t>(high - low)));
    MEMORY_BASIC_INFORMATION above{};
    if (VirtualQuery(high, &above, sizeof(above)) == sizeof(above) && above.State != MEM_COMMIT) {
        Require(!GuestCopyIn_nid_no_patch(copy, high - 8, 16));
        Require(!GuestCopyOut_nid_no_patch(high - 8, source, 16));
        Require(!GuestWritable_nid_no_patch(high - 8, 16));
    }
}

static DWORD WINAPI StackThread(void*) {
    CheckCurrentStack();
    return 0;
}

static void CheckGuestOwnedStack() {
    constexpr std::size_t bytes = 65536;
    constexpr std::size_t page = 4096;
    auto* stack = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, bytes));
    Require(stack != nullptr);
    GuestArena::GuestArenaCommit_nid_postfix(stack, bytes, PAGE_READWRITE, bytes);
    GuestStacks::GuestStackRegister_nid_no_patch(stack, bytes);
    Require(VirtualFree(stack + 4 * page, page, MEM_DECOMMIT));
    unsigned char copy[16] = {};
    const unsigned char source[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const auto host = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
    GuestStacks::GuestStackSwitch_nid_no_patch(reinterpret_cast<std::uintptr_t>(stack + bytes - 64));
    const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const bool switched = tib->StackLimit == stack && tib->StackBase == stack + bytes;
    const bool readUnmapped = GuestCopyIn_nid_no_patch(copy, stack + 4 * page, 8);
    const bool writeUnmapped = GuestCopyOut_nid_no_patch(stack + 4 * page + 8, source, 8);
    const bool writableUnmapped = GuestWritable_nid_no_patch(stack + 4 * page - 8, 16);
    const bool readMapped = GuestCopyIn_nid_no_patch(copy, stack + 8 * page, 8);
    GuestStacks::GuestStackSwitch_nid_no_patch(host);
    Require(switched);
    Require(!readUnmapped && !writeUnmapped && !writableUnmapped && readMapped);
}

int main() {
    errno = 71;
    CheckCurrentStack();
    const HANDLE thread = CreateThread(nullptr, 1 << 20, StackThread, nullptr, 0, nullptr);
    Require(thread != nullptr);
    Require(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
    DWORD code = 1;
    Require(GetExitCodeThread(thread, &code) && code == 0);
    CloseHandle(thread);
    CheckGuestOwnedStack();
    Require(errno == 71);
    std::puts("guest copy stack tests passed");
    return 0;
}
