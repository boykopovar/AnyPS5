#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

extern "C" {
std::uint32_t* APS5_VABI sceAgcAcbDmaData(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint64_t, std::uint8_t, std::uint8_t, std::uint64_t, std::uint32_t, std::uint8_t, std::uint8_t);
std::uint32_t APS5_VABI sceAgcAcbDmaDataGetSize();
std::uint32_t* APS5_VABI sceAgcDcbDmaData(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint8_t, std::uint64_t, std::uint8_t, std::uint8_t, std::uint64_t, std::uint32_t, std::uint8_t, std::uint8_t, std::uint8_t);
std::uint32_t APS5_VABI sceAgcDcbDmaDataGetSize();
}

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;
constexpr std::uint64_t Destination = 0x0000778899aabbccull;
constexpr std::uint64_t Source = 0x0000112233445566ull;

using Testing::Require;

template<typename TAction>
void ExpectFailure(TAction action) {
    Testing::RequireThrows<std::runtime_error>(action, "expected invalid input to fail");
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
void VerifyCommand(TWriter writer, TSize size, std::uint32_t control) {
    const std::array expected{0xc0055000u, control, 0x33445566u, 0x1122u, 0x99aabbccu, 0x7788u, 0xd8001234u};
    const auto count = static_cast<std::uint32_t>(expected.size());
    Require(size() == count * sizeof(std::uint32_t), "size query does not match the DMA packet");

    Storage storage;
    auto* packet = writer(&storage.buffer);
    Require(packet == storage.words.data() && std::equal(expected.begin(), expected.end(), packet), "incorrect DMA packet");
    Require(storage.buffer.cursor_up == packet + size() / sizeof(std::uint32_t), "cursor advance does not match the size query");
    Require(std::all_of(storage.words.begin() + count, storage.words.end(), [](std::uint32_t word) { return word == Sentinel; }), "DMA command overwrote following words");

    Storage exact;
    exact.limitTo(size() / sizeof(std::uint32_t));
    Require(writer(&exact.buffer) == exact.words.data() && exact.buffer.cursor_up == exact.buffer.cursor_down, "DMA packet does not fill the queried size");

    Storage shortBuffer;
    shortBuffer.limitTo(size() / sizeof(std::uint32_t) - 1u);
    const auto before = shortBuffer.words;
    ExpectFailure([&] { writer(&shortBuffer.buffer); });
    Require(shortBuffer.words == before && shortBuffer.buffer.cursor_up == shortBuffer.words.data(), "failed DMA write modified the buffer");
}

}

namespace {

const Testing::Case compute{"AcbDmaData_Buffers_WritesSizedPacketOrRejectsShortBuffer", [] {
    VerifyCommand(writeCompute, sceAgcAcbDmaDataGetSize, 0x44102000u);
}};

const Testing::Case draw{"DcbDmaData_Buffers_WritesSizedPacketOrRejectsShortBuffer", [] {
    VerifyCommand(writeDraw, sceAgcDcbDmaDataGetSize, 0xc4102001u);
}};

const Testing::Case sizes{"DmaDataGetSize_ComputeAndDraw_AreEqual", [] {
    Require(sceAgcAcbDmaDataGetSize() == sceAgcDcbDmaDataGetSize(), "compute and draw DMA sizes differ");
}};

} // namespace
