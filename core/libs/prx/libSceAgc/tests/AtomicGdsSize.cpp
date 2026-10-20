#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <cstdio>
#include <cstdint>

extern "C" std::uint32_t APS5_VABI sceAgcAcbAtomicGdsGetSize();
extern "C" std::uint32_t* APS5_VABI sceAgcAcbAtomicGds_0900(CommandBuffer* buf, std::uint32_t control, std::uint32_t mode,
    std::uint32_t gdsOp, std::uint32_t src, std::uint16_t offset, std::uint16_t index, std::uint32_t mask,
    std::uint32_t data, std::uint64_t compareData, std::uint64_t extraData);
extern "C" std::uint32_t* APS5_VABI sceAgcAcbAtomicGds(CommandBuffer* buf, std::uint32_t control, std::uint32_t mode,
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
    std::array<std::uint32_t, 16> unversionedWords{};
    CommandBuffer unversionedBuffer{unversionedWords.data(), unversionedWords.data() + unversionedWords.size(), unversionedWords.data(), unversionedWords.data() + unversionedWords.size(), nullptr, nullptr, 0};
    const auto* versioned = sceAgcAcbAtomicGds_0900(&buffer, 0x2e1u, 1, 1, 7, 0x100, 0x200, 0x00ff00ffu, 0x12345678u, 0x1122334455667788ull, 0x99aabbccddeeff00ull);
    const auto* unversioned = sceAgcAcbAtomicGds(&unversionedBuffer, 0x2e1u, 1, 1, 7, 0x100, 0x200, 0x00ff00ffu, 0x12345678u, 0x1122334455667788ull, 0x99aabbccddeeff00ull);
    if (unversioned != unversionedWords.data() || unversionedBuffer.cursor_up != unversionedWords.data() + 11 || versioned[1] != 0x001500e1u || unversioned[1] != 0x00150061u) {
        std::fprintf(stderr, "unversioned ACB atomic GDS must clear operation bit 7\n");
        return 1;
    }
    for (int i = 2; i < 11; ++i) {
        if (versioned[i] != unversioned[i]) {
            std::fprintf(stderr, "versioned and unversioned ACB atomic GDS emitted different payloads\n");
            return 1;
        }
    }
}
