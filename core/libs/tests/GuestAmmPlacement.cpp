#include "prx/libSceAmpr/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        std::uintptr_t arenaBase = 0;
        std::size_t arenaBytes = 0;
        GuestArena::GuestArenaRange_nid_postfix(&arenaBase, &arenaBytes);
        Require(arenaBase != 0 && arenaBytes != 0, "the guest arena is unavailable");
        const std::uint64_t arenaEnd = arenaBase + arenaBytes;
        constexpr std::uint64_t occupied = 4ull << 20;
        GuestArena::GuestArenaMarkUsed_nid_postfix(reinterpret_cast<void*>(arenaEnd - occupied), occupied);

        std::uint64_t start = 0;
        std::uint64_t end = 0;
        std::uint64_t multimapStart = 0;
        std::uint64_t multimapEnd = 0;
        Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0, "AMM ranges were not reported");
        Require(end - start == 256ull << 30 && end == multimapStart && multimapEnd - multimapStart == 32ull << 30, "AMM ranges are not a 256 GiB range and an adjacent 32 GiB multimap range");
        Require(start >= arenaBase && multimapEnd <= arenaEnd - occupied, "AMM ranges overlap the used top of the guest arena");
        Require(start % (64ull << 30) == 0, "the AMM range does not start on a 64 GiB boundary");
        Require(GuestArena::GuestArenaHighestFree_nid_no_patch(multimapEnd - start, 64ull << 30) < start, "AMM ranges are not at the top of the free guest arena");
        std::puts("AMM ranges placed below the used top of the guest arena");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
