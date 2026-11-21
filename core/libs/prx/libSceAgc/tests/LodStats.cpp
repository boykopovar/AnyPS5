#include "prx/libSceAgc/DcbDraw/include/Stats.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

struct Storage {
    std::array<std::uint32_t, 64> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
};

int fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

}

int main() {
    alignas(64) static std::array<std::uint8_t, 0x840> report{};
    const auto address = reinterpret_cast<std::uintptr_t>(report.data());
    for (const std::uint8_t policy : {0u, 2u}) {
        Storage dcb;
        const auto* first = sceAgcDcbGetLodStats(&dcb.buffer, policy, nullptr, 0, 0, 0, 0, 0);
        const auto* second = sceAgcDcbGetLodStats(&dcb.buffer, policy, report.data(), 0x840, 0, 0, 1, 0);
        if (first != dcb.words.data() || second != dcb.words.data() + 5 || dcb.buffer.cursor_up != dcb.words.data() + 10) return fail("LOD stats packets are not five words each");
        const std::array<std::uint32_t, 5> expectedFirst{0xc0038e00u, 0, 0, 0, static_cast<std::uint32_t>(policy) << 28u};
        const std::array<std::uint32_t, 5> expectedSecond{0xc0038e00u, 0x840u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), (static_cast<std::uint32_t>(policy) << 28u) | (1u << 19u)};
        for (std::size_t index = 0; index < 5; ++index) {
            if (dcb.words[index] != expectedFirst[index]) return fail("the sizing LOD stats packet differs");
            if (dcb.words[5 + index] != expectedSecond[index]) return fail("the reporting LOD stats packet differs");
        }
        Storage fields;
        sceAgcDcbGetLodStats(&fields.buffer, 3, report.data(), 0x40, 0xff, 1, 0, 0xff);
        if (fields.words[4] != ((3u << 28u) | (1u << 18u) | (0xffu << 10u) | (0xffu << 2u))) return fail("the LOD stats control word lost a field");
    }
    struct Measured {
        std::uint8_t policy;
        std::uintptr_t buffer;
        std::uint32_t size;
        std::uint32_t resetCount;
        std::uint8_t forceReset;
        std::array<std::uint32_t, 4> expected;
    };
    const std::array<Measured, 3> measured{{
        {0x11, 0x22222222u, 0x33333333u, 0x44444444u, 0x55, {0x33333333u, 0x22222200u, 0u, 0x10051000u}},
        {1, 2, 3, 4, 5, {3u, 0u, 0u, 0x10041000u}},
        {0x22, 0x3333333344444444u, 0, 0, 0, {0u, 0x44444440u, 0x33333333u, 0x20000000u}},
    }};
    for (const auto& row : measured) {
        Storage dcb;
        sceAgcDcbGetLodStats(&dcb.buffer, row.policy, reinterpret_cast<const volatile void*>(row.buffer), row.size, row.resetCount, row.forceReset, 0, 0);
        if (dcb.words[0] != 0xc0038e00u) return fail("the LOD stats header differs from the console");
        for (std::size_t index = 0; index < 4; ++index) {
            if (dcb.words[1 + index] != row.expected[index]) return fail("the LOD stats packet differs from the console measurement");
        }
    }
    std::puts("AGC LOD stats tests passed");
    return 0;
}
