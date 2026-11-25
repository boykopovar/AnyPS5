#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "System reserved hint check failed at line %u\n", static_cast<unsigned>(location.line()));
        std::abort();
    }
}

static bool OverlapsSystemReserved(const void* pointer, std::size_t bytes) {
    constexpr std::uintptr_t start = 0x00000007FFFFC000ull;
    constexpr std::uintptr_t end = 0x0000001000000000ull;
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    return address < end && address + bytes > start;
}

int main() {
    constexpr std::size_t bufferBytes = 64u << 20;
    constexpr std::size_t heapBytes = std::size_t{24} << 30;
    constexpr std::size_t ordinaryBytes = 16u << 20;
    const auto failed = reinterpret_cast<void*>(~std::uintptr_t{0});
    auto* const bufferHint = reinterpret_cast<unsigned char*>(0x900000000ull);
    auto* const heapHint = reinterpret_cast<void*>(0x800000000ull);
    auto* const belowHint = reinterpret_cast<void*>(0x7FFFF0000ull);

    void* heap = mmap_nid_postfix(heapHint, heapBytes, 0, 0x1002, -1, 0);
    std::fprintf(stderr, "System reserved hint: heap=%p\n", heap);
    Require(heap == heapHint);
    Require(munmap_nid_postfix(heap, heapBytes) == 0);

    auto* buffer = static_cast<unsigned char*>(mmap_nid_postfix(bufferHint, bufferBytes, 3, 0x1002, -1, 0));
    std::fprintf(stderr, "System reserved hint: buffer=%p\n", static_cast<void*>(buffer));
    Require(buffer == bufferHint);
    Require(GuestArena::GuestArenaContains_nid_postfix(buffer, bufferBytes));
    buffer[0] = 0x31;
    buffer[bufferBytes - 1] = 0x32;
    Require(buffer[0] == 0x31 && buffer[bufferBytes - 1] == 0x32);

    void* ordinary = mmap_nid_postfix(nullptr, ordinaryBytes, 3, 0x1002, -1, 0);
    Require(ordinary != failed && !OverlapsSystemReserved(ordinary, ordinaryBytes));

    void* below = mmap_nid_postfix(belowHint, bufferBytes, 3, 0x1002, -1, 0);
    std::fprintf(stderr, "System reserved hint: below=%p\n", below);
    Require(below != failed && !OverlapsSystemReserved(below, bufferBytes));

    Require(munmap_nid_postfix(buffer, bufferBytes) == 0);
    auto* again = static_cast<unsigned char*>(mmap_nid_postfix(bufferHint, bufferBytes, 3, 0x1002, -1, 0));
    Require(again == bufferHint);
    Require(again[0] == 0);

    Require(munmap_nid_postfix(again, bufferBytes) == 0);
    Require(munmap_nid_postfix(below, bufferBytes) == 0);
    Require(munmap_nid_postfix(ordinary, ordinaryBytes) == 0);
}
