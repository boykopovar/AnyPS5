#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

extern "C" {
std::uint32_t* APS5_VABI sceAgcAcbPrimeUtcl2(CommandBuffer*, const volatile void*, std::uint32_t);
std::uint32_t APS5_VABI sceAgcAcbPrimeUtcl2GetSize();
std::uint32_t* APS5_VABI sceAgcDcbPrimeUtcl2(CommandBuffer*, const volatile void*, std::uint32_t);
std::uint32_t APS5_VABI sceAgcDcbPrimeUtcl2GetSize();
}

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;
constexpr std::uint64_t Address = 0x0000112233440000ull;
constexpr std::uint32_t SizeInBytes = 0x00345000u;

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
    return sceAgcAcbPrimeUtcl2(buffer, reinterpret_cast<const volatile void*>(Address), SizeInBytes);
}

std::uint32_t* writeDraw(CommandBuffer* buffer) {
    return sceAgcDcbPrimeUtcl2(buffer, reinterpret_cast<const volatile void*>(Address), SizeInBytes);
}

template <typename TWriter, typename TSize>
void VerifyCommand(TWriter writer, TSize size) {
    const std::array expected{0xc0031000u, 0u, 0x33440000u, 0x1122u, SizeInBytes};
    const auto count = static_cast<std::uint32_t>(expected.size());
    Require(size() == count * sizeof(std::uint32_t), "size query does not match the prime packet");

    Storage storage;
    auto* packet = writer(&storage.buffer);
    Require(packet == storage.words.data() && std::equal(expected.begin(), expected.end(), packet), "incorrect prime packet");
    Require(storage.buffer.cursor_up == packet + size() / sizeof(std::uint32_t), "cursor advance does not match the size query");
    Require(std::all_of(storage.words.begin() + count, storage.words.end(), [](std::uint32_t word) { return word == Sentinel; }), "prime command overwrote following words");

    auto* second = writer(&storage.buffer);
    Require(second == packet + count && std::equal(expected.begin(), expected.end(), second), "second prime packet does not follow the first");

    Storage exact;
    exact.limitTo(size() / sizeof(std::uint32_t));
    Require(writer(&exact.buffer) == exact.words.data() && exact.buffer.cursor_up == exact.buffer.cursor_down, "prime packet does not fill the queried size");

    Storage shortBuffer;
    shortBuffer.limitTo(size() / sizeof(std::uint32_t) - 1u);
    const auto before = shortBuffer.words;
    ExpectFailure([&] { writer(&shortBuffer.buffer); });
    Require(shortBuffer.words == before && shortBuffer.buffer.cursor_up == shortBuffer.words.data(), "failed prime write modified the buffer");

    ExpectFailure([&] { writer(nullptr); });
}

}

namespace {

const Testing::Case compute{"AcbPrimeUtcl2_Buffers_WritesSizedPacketOrRejectsShortBuffer", [] {
    VerifyCommand(writeCompute, sceAgcAcbPrimeUtcl2GetSize);
}};

const Testing::Case draw{"DcbPrimeUtcl2_Buffers_WritesSizedPacketOrRejectsShortBuffer", [] {
    VerifyCommand(writeDraw, sceAgcDcbPrimeUtcl2GetSize);
}};

const Testing::Case sizes{"PrimeUtcl2GetSize_ComputeAndDraw_AreEqual", [] {
    Require(sceAgcAcbPrimeUtcl2GetSize() == sceAgcDcbPrimeUtcl2GetSize(), "compute and draw prime sizes differ");
}};

} // namespace
