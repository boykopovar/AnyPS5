#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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
    return sceAgcAcbPrimeUtcl2(buffer, reinterpret_cast<const volatile void*>(Address), SizeInBytes);
}

std::uint32_t* writeDraw(CommandBuffer* buffer) {
    return sceAgcDcbPrimeUtcl2(buffer, reinterpret_cast<const volatile void*>(Address), SizeInBytes);
}

template <typename TWriter, typename TSize>
void testCommand(TWriter writer, TSize size) {
    const std::array expected{0xc0031000u, 0u, 0x33440000u, 0x1122u, SizeInBytes};
    const auto count = static_cast<std::uint32_t>(expected.size());
    check(size() == count * sizeof(std::uint32_t), "size query does not match the prime packet");

    Storage storage;
    auto* packet = writer(&storage.buffer);
    check(packet == storage.words.data() && std::equal(expected.begin(), expected.end(), packet), "incorrect prime packet");
    check(storage.buffer.cursor_up == packet + size() / sizeof(std::uint32_t), "cursor advance does not match the size query");
    check(std::all_of(storage.words.begin() + count, storage.words.end(), [](std::uint32_t word) { return word == Sentinel; }), "prime command overwrote following words");

    auto* second = writer(&storage.buffer);
    check(second == packet + count && std::equal(expected.begin(), expected.end(), second), "second prime packet does not follow the first");

    Storage exact;
    exact.limitTo(size() / sizeof(std::uint32_t));
    check(writer(&exact.buffer) == exact.words.data() && exact.buffer.cursor_up == exact.buffer.cursor_down, "prime packet does not fill the queried size");

    Storage shortBuffer;
    shortBuffer.limitTo(size() / sizeof(std::uint32_t) - 1u);
    const auto before = shortBuffer.words;
    expectFailure([&] { writer(&shortBuffer.buffer); });
    check(shortBuffer.words == before && shortBuffer.buffer.cursor_up == shortBuffer.words.data(), "failed prime write modified the buffer");

    expectFailure([&] { writer(nullptr); });
}

}

int main() {
    try {
        testCommand(writeCompute, sceAgcAcbPrimeUtcl2GetSize);
        testCommand(writeDraw, sceAgcDcbPrimeUtcl2GetSize);
        check(sceAgcAcbPrimeUtcl2GetSize() == sceAgcDcbPrimeUtcl2GetSize(), "compute and draw prime sizes differ");
        std::puts("AGC prime UTCL2 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
