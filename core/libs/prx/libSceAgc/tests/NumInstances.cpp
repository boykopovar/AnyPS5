#include "prx/libSceAgc/DcbDraw/include/Instancing.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

int main() {
    if (sceAgcDcbSetNumInstancesGetSize() != 8) {
        std::fprintf(stderr, "instance count size is not 8 bytes\n");
        return 1;
    }
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    constexpr std::array<std::uint32_t, 3> counts{1u, 2u, 1000u};
    for (std::size_t index = 0; index < counts.size(); ++index) {
        const auto* packet = sceAgcDcbSetNumInstances(&buffer, counts[index]);
        if (packet != words.data() + index * 2u || buffer.cursor_up != words.data() + (index + 1u) * 2u) {
            std::fprintf(stderr, "instance count %u was not written as one 2-dword packet after the previous one\n", counts[index]);
            return 1;
        }
        if (packet[0] != 0xc0002f00u || packet[1] != counts[index]) {
            std::fprintf(stderr, "instance count %u wrote 0x%08x 0x%08x, not NUM_INSTANCES with the count\n", counts[index], packet[0], packet[1]);
            return 1;
        }
    }
    if (words[6] != 0u) {
        std::fprintf(stderr, "instance count packets wrote past their two dwords\n");
        return 1;
    }
    std::puts("AGC instance count tests passed");
    return 0;
}
