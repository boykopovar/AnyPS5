#include "prx/libSceAgcDriver/Graphics/include/PushDataShadow.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

using AgcDriver::Graphics::PushDataShadow;
constexpr std::size_t Bytes = 256;
using Block = std::array<std::byte, Bytes>;
int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAILED: %s\n", what.c_str());
    ++failures;
}

void SetWord(Block& block, std::size_t word, std::uint32_t value) {
    std::memcpy(block.data() + word * 4, &value, sizeof(value));
}

VkCommandBuffer Commands(std::uintptr_t value) {
    return reinterpret_cast<VkCommandBuffer>(value);
}

void testRanges() {
    PushDataShadow<Bytes> shadow;
    Block block{};
    for (std::size_t word = 0; word < Bytes / 4; ++word) SetWord(block, word, 0x1000u + static_cast<std::uint32_t>(word));
    auto range = shadow.Changed(Commands(1), 7, block);
    Expect(range.first == 0 && range.second == Bytes, "the first push of a command buffer covers the whole block");
    shadow.Pushed(8);
    range = shadow.Changed(Commands(1), 8, block);
    Expect(range.first == range.second, "an unchanged block pushes nothing");
    SetWord(block, 56, 0xbeefu);
    range = shadow.Changed(Commands(1), 8, block);
    Expect(range.first == 224 && range.second == 232, "a change to the low dword of an address pushes the whole address");
    shadow.Pushed(9);
    SetWord(block, 5, 0x1u);
    SetWord(block, 41, 0x2u);
    range = shadow.Changed(Commands(1), 9, block);
    Expect(range.first == 16 && range.second == 168, "two changes push one range from the first to the last changed address");
    shadow.Pushed(10);
    range = shadow.Changed(Commands(1), 11, block);
    Expect(range.first == 0 && range.second == Bytes, "a push by anything else since the last one pushes the whole block");
    shadow.Pushed(12);
    range = shadow.Changed(Commands(2), 12, block);
    Expect(range.first == 0 && range.second == Bytes, "another command buffer gets the whole block");
}

void testKeptBytes() {
    PushDataShadow<Bytes> shadow;
    Block block{};
    shadow.Changed(Commands(3), 1, block);
    shadow.Pushed(2);
    std::uint64_t mark = 2;
    std::uint64_t seed = 0x9e3779b97f4a7c15ull;
    Block gpu = block;
    for (int round = 0; round < 4000; ++round) {
        seed ^= seed << 13u;
        seed ^= seed >> 7u;
        seed ^= seed << 17u;
        const auto changes = seed % 4u;
        for (std::uint64_t change = 0; change < changes; ++change) SetWord(block, (seed >> (8u * change + 4u)) % (Bytes / 4), static_cast<std::uint32_t>(seed >> 32u) + static_cast<std::uint32_t>(change));
        const auto [first, end] = shadow.Changed(Commands(3), mark, block);
        Expect(first % 8u == 0u && end % 8u == 0u && first <= end && end <= Bytes, "pushed ranges are whole addresses inside the block");
        std::memcpy(gpu.data() + first, block.data() + first, end - first);
        if (first < end) shadow.Pushed(++mark);
        if (gpu != block) {
            Expect(false, "the pushed ranges rebuild the block in round " + std::to_string(round));
            return;
        }
    }
}

}

int main() {
    testRanges();
    testKeptBytes();
    if (failures != 0) return EXIT_FAILURE;
    std::puts("push data shadow tests passed");
    return EXIT_SUCCESS;
}
