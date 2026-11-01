#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"
#include "prx/libSceAgc/Acb/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/Patch/include/Dma.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;
constexpr std::uint64_t Destination = 0x0000778899aabbccull;
constexpr std::uint64_t Source = 0x0000112233445566ull;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error("expected invalid input to fail");
}

struct Storage {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};

    Storage() {
        words.fill(Sentinel);
    }

    void limitTo(std::uint32_t count) {
        buffer.cursor_down = buffer.cursor_up + count;
    }
};

std::uint32_t* writeCompute(CommandBuffer* buffer) {
    return sceAgcAcbDmaData(buffer, 5, 2, Destination, 10, 1, Source, 0x1234u, 1, 1);
}

std::uint32_t* writeDraw(CommandBuffer* buffer) {
    return sceAgcDcbDmaData(buffer, 1, 5, 2, Destination, 10, 1, Source, 0x1234u, 1, 1, 1);
}

template <typename TWriter, typename TSize>
void testCommand(TWriter writer, TSize size, std::uint32_t control) {
    const std::array expected{0xc0055000u, control, 0x33445566u, 0x1122u, 0x99aabbccu, 0x7788u, 0xd8001234u};
    const auto count = static_cast<std::uint32_t>(expected.size());
    check(size() == count * sizeof(std::uint32_t), "size query does not match the DMA packet");

    Storage storage;
    auto* packet = writer(&storage.buffer);
    check(packet == storage.words.data() && std::equal(expected.begin(), expected.end(), packet), "incorrect DMA packet");
    check(storage.buffer.cursor_up == packet + size() / sizeof(std::uint32_t), "cursor advance does not match the size query");
    check(std::all_of(storage.words.begin() + count, storage.words.end(), [](std::uint32_t word) { return word == Sentinel; }), "DMA command overwrote following words");

    Storage exact;
    exact.limitTo(size() / sizeof(std::uint32_t));
    check(writer(&exact.buffer) == exact.words.data() && exact.buffer.cursor_up == exact.buffer.cursor_down, "DMA packet does not fill the queried size");

    Storage shortBuffer;
    shortBuffer.limitTo(size() / sizeof(std::uint32_t) - 1u);
    const auto before = shortBuffer.words;
    expectFailure([&] { writer(&shortBuffer.buffer); });
    check(shortBuffer.words == before && shortBuffer.buffer.cursor_up == shortBuffer.words.data(), "failed DMA write modified the buffer");
}

void testPatches() {
    constexpr std::uint64_t PatchedDestination = 0x0000aabbccddeeffull;
    constexpr std::uint64_t PatchedSource = 0x0000102030405060ull;
    for (auto* writer : {writeCompute, writeDraw}) {
        Storage storage;
        auto* packet = writer(&storage.buffer);
        auto expected = storage.words;
        check(sceAgcDmaDataPatchSetDstAddressOrOffset(packet, PatchedDestination) == 0, "the destination patch failed");
        expected[4] = 0xccddeeffu;
        expected[5] = 0xaabbu;
        check(storage.words == expected, "the destination patch did not replace only words 4 and 5");
        check(sceAgcDmaDataPatchSetSrcAddressOrOffsetOrImmediate(packet, PatchedSource) == 0, "the source patch failed");
        expected[2] = 0x30405060u;
        expected[3] = 0x1020u;
        check(storage.words == expected, "the source patch did not replace only words 2 and 3");
        check(sceAgcDmaDataPatchSetSrcAddressOrOffsetOrImmediate(packet, 0xfedcba9876543210ull) == 0 && packet[2] == 0x76543210u && packet[3] == 0xfedcba98u, "the source patch did not keep a 64-bit immediate whole");
    }

}

}

int main() {
    try {
        testCommand(writeCompute, sceAgcAcbDmaDataGetSize, 0x44102000u);
        testCommand(writeDraw, sceAgcDcbDmaDataGetSize, 0xc4102001u);
        check(sceAgcAcbDmaDataGetSize() == sceAgcDcbDmaDataGetSize(), "compute and draw DMA sizes differ");
        testPatches();
        std::puts("AGC DMA data tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
