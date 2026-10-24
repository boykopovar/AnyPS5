#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" void APS5_VABI sceJemspaceGetStats(const void*, void*);

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

constexpr std::array<std::uint64_t, 5> counters{
    0x0102030405060708ULL, 0x1122334455667788ULL, 0x2132435465768798ULL,
    0x31425364758697a8ULL, 0x415263748596a7b8ULL
};

std::array<std::byte, 192> MakeSpace() {
    std::array<std::byte, 192> space;
    space.fill(std::byte{0xa5});
    std::memcpy(space.data() + 104, counters.data(), sizeof(counters));
    return space;
}

void CheckCopy() {
    const auto space = MakeSpace();
    auto original = space;
    std::array<std::byte, 42> output;
    output.fill(std::byte{0x5a});
    sceJemspaceGetStats(space.data(), output.data() + 1);
    Require(std::memcmp(output.data() + 1, counters.data(), sizeof(counters)) == 0,
            "Statistics must preserve all five 64-bit values in order");
    Require(output.front() == std::byte{0x5a} && output.back() == std::byte{0x5a},
            "Statistics must write exactly 40 bytes");
    Require(space == original, "A disjoint statistics query must not modify its context");
}

void CheckNulls() {
    auto space = MakeSpace();
    const auto original = space;
    sceJemspaceGetStats(nullptr, space.data());
    Require(space == original, "A null context must leave output unchanged");
    sceJemspaceGetStats(space.data(), nullptr);
    Require(space == original, "A null output must leave the context unchanged");
    sceJemspaceGetStats(nullptr, nullptr);
}

void CheckOverlap() {
    for (const std::size_t offset : {104U, 112U, 128U}) {
        auto space = MakeSpace();
        const auto original = space;
        auto expected = counters;
        if (offset == 112) {
            expected[4] = counters[3];
        } else if (offset == 128) {
            expected[4] = counters[1];
        }
        sceJemspaceGetStats(space.data(), space.data() + offset);
        Require(std::memcmp(space.data() + offset, expected.data(), sizeof(expected)) == 0,
                "Overlapping output must preserve the observed snapshot ordering");
        Require(std::memcmp(space.data(), original.data(), offset) == 0,
                "Overlapping output must preserve bytes before the destination");
        Require(std::memcmp(space.data() + offset + 40, original.data() + offset + 40,
                            space.size() - offset - 40) == 0,
                "Overlapping output must preserve bytes after the destination");
    }
}

}

int main() {
    CheckCopy();
    CheckNulls();
    CheckOverlap();
}
