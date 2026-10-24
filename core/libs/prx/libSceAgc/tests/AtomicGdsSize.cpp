#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <cstdio>
#include <cstdint>

extern "C" std::uint32_t APS5_VABI sceAgcAcbAtomicGdsGetSize();
extern "C" std::uint32_t* APS5_VABI sceAgcAcbAtomicGds_0900(CommandBuffer* buf, std::uint32_t control, std::uint32_t mode,
    std::uint32_t gdsOp, std::uint32_t src, std::uint16_t offset, std::uint16_t index, std::uint32_t mask,
    std::uint32_t data, std::uint64_t compareData, std::uint64_t extraData);

int main() {
    constexpr std::uint32_t expectedSize = 11u * sizeof(std::uint32_t);
    if (sceAgcAcbAtomicGdsGetSize() != expectedSize) {
        std::fprintf(stderr, "ACB atomic GDS size does not match its 11-dword packet\n");
        return 1;
    }
    std::array<std::uint32_t, 64> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    const auto* packet = sceAgcAcbAtomicGds_0900(&buffer, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (packet != words.data() || buffer.cursor_up != words.data() + expectedSize / sizeof(std::uint32_t)) {
        std::fprintf(stderr, "ACB atomic GDS builder wrote a different size than its size accessor\n");
        return 1;
    }
}
