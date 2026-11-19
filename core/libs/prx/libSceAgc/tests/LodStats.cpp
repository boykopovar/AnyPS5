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
        const std::array<std::uint32_t, 5> expectedFirst{0xc0031000u, 0, 0, 0, static_cast<std::uint32_t>(policy) << 28u};
        const std::array<std::uint32_t, 5> expectedSecond{0xc0031000u, 0x840u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), (static_cast<std::uint32_t>(policy) << 28u) | (1u << 19u)};
        for (std::size_t index = 0; index < 5; ++index) {
            if (dcb.words[index] != expectedFirst[index]) return fail("the sizing LOD stats packet differs");
            if (dcb.words[5 + index] != expectedSecond[index]) return fail("the reporting LOD stats packet differs");
        }
        Storage fields;
        sceAgcDcbGetLodStats(&fields.buffer, 3, report.data(), 0x40, 0xff, 1, 0, 0xff);
        if (fields.words[4] != ((3u << 28u) | (1u << 18u) | (0xffu << 10u) | (0xffu << 2u))) return fail("the LOD stats control word lost a field");
    }
    std::puts("AGC LOD stats tests passed");
    return 0;
}
