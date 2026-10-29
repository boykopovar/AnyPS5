#include "prx/libSceAgc/DcbDraw/include/DrawIndexed.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

namespace {

struct Call {
    std::uint32_t first;
    std::uint64_t second;
    std::uint64_t third;
    std::uint32_t fourth;
    std::uint64_t modifier;
};

std::array<std::uint32_t, 15> expected(const Call& call) {
    return {0xc0017904u, 0x342u, 0xc6000006u, 0xc0073a00u, call.first, static_cast<std::uint32_t>(call.second), static_cast<std::uint32_t>(call.second >> 32u),
            call.fourth == 0 ? 1u : call.fourth, static_cast<std::uint32_t>(call.third), static_cast<std::uint32_t>(call.third >> 32u), call.fourth, 0x80u,
            0xc0017904u, 0x342u, 0xc6000000u};
}

}

int main() {
    if (sceAgcDcbDrawIndexMultiInstancedGetSize() != 60) {
        std::fprintf(stderr, "multi-instanced draw size is not 60 bytes\n");
        return 1;
    }
    constexpr std::uint64_t ia = 0x123456789a00ull;
    constexpr std::uint64_t oa = 0x222233334440ull;
    const std::array<Call, 12> calls{{
        {100, ia, 0, 3, 0}, {100, ia, 0, 1, 0}, {0, ia, 0, 3, 0}, {100, ia, oa, 3, 0},
        {100, ia, oa, 3, 0x10}, {100, ia, oa, 3, 0x8 | (3ull << 24)}, {100, ia, oa, 3, 0x1 | (2ull << 9) | 0x4 | (5ull << 19)}, {100, ia, oa, 3, 0x100000000ull},
        {100, 0, 0, 3, 0}, {100, ia + 1, oa + 2, 3, 0}, {100, ia, oa, 0, 0}, {0xffffffffu, ia, oa, 0xffffffffu, 0},
    }};
    for (const auto& call : calls) {
        std::array<std::uint32_t, 32> words{};
        CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
        const auto* packet = sceAgcDcbDrawIndexMultiInstanced(&buffer, call.first, reinterpret_cast<const volatile void*>(call.second), reinterpret_cast<const volatile void*>(call.third), call.fourth, call.modifier);
        const auto want = expected(call);
        bool same = packet == words.data() && buffer.cursor_up == words.data() + want.size();
        for (std::size_t i = 0; same && i < want.size(); ++i) same = words[i] == want[i];
        if (!same) {
            std::fprintf(stderr, "multi-instanced draw words differ from the console's for (0x%x, 0x%llx, 0x%llx, 0x%x, 0x%llx)\n", call.first, static_cast<unsigned long long>(call.second), static_cast<unsigned long long>(call.third), call.fourth, static_cast<unsigned long long>(call.modifier));
            return 1;
        }
    }
    std::puts("AGC multi-instanced draw tests passed");
    return 0;
}
