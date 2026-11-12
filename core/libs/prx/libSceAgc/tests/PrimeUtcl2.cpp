#include "prx/libSceAgc/DcbState/include/Display.hpp"
#include "prx/libSceAgc/Acb/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <stdexcept>

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;
constexpr std::uint64_t Address = 0x0000112233440000ull;
constexpr std::uint32_t PageCount = 0x00345000u;

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
    return sceAgcAcbPrimeUtcl2(buffer, 3u, 1u, reinterpret_cast<const volatile void*>(Address), PageCount);
}

std::uint32_t* writeDraw(CommandBuffer* buffer) {
    return sceAgcDcbPrimeUtcl2(buffer, 3u, 1u, reinterpret_cast<const volatile void*>(Address), PageCount);
}

template <typename TWriter, typename TSize>
void testCommand(TWriter writer, TSize size, bool compute) {
    const std::array expected{0xc0035d00u, compute ? 11u : 0x4000000bu, 0x33440000u, 0x1122u, 0x1000u};
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

void testReferencePackets() {
    struct Case {
        std::uint32_t permission;
        std::uint32_t mode;
        std::uint64_t address;
        std::uint32_t pages;
        std::array<std::uint32_t, 4> payload;
    };
    const Case cases[]{
        {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, {1u, 0x33330000u, 0u, 0x444u}},
        {1u, 2u, 3u, 4u, {1u, 0u, 0u, 4u}},
        {0x22222222u, 0x44444444u, 0u, 0u, {2u, 0u, 0u, 0u}},
        {0xffffffffu, 0xffffffffu, 0xffff123456789abfull, 0xffffffffu, {15u, 0x56788000u, 0xffff1234u, 0x3fffu}},
        {0u, 0u, 0x100003fffull, 0x4000u, {0u, 0u, 1u, 0u}},
        {0u, 1u, 0x4000u, 1u, {8u, 0x4000u, 0u, 1u}},
    };
    for (const auto compute : {true, false}) {
        const auto writer = compute ? sceAgcAcbPrimeUtcl2 : sceAgcDcbPrimeUtcl2;
        for (const auto& item : cases) {
            Storage storage;
            const auto* packet = writer(&storage.buffer, item.permission, item.mode, reinterpret_cast<const volatile void*>(item.address), item.pages);
            auto expected = item.payload;
            if (!compute) expected[0] |= 0x40000000u;
            check(packet == storage.words.data() && packet[0] == 0xc0035d00u, "reference prime header or result differs");
            check(std::equal(expected.begin(), expected.end(), packet + 1), "reference prime payload differs");
            check(storage.buffer.cursor_up == packet + 5 && storage.words[5] == Sentinel, "reference prime cursor or sentinel differs");
            AgcDriver::Pm4::Validate(std::span(packet, 5), compute ? 0x20u : 0u);
            check(AgcDriver::Pm4::KnownName(packet[0]) == "PRIME_UTCL2", "prime opcode has no name");
        }
    }
}

void testPacketValidation() {
    const std::array valid{0xc0035d00u, 15u, 0x4000u, 0x1122u, 0x3fffu};
    for (const auto queue : {0u, 0x20u}) AgcDriver::Pm4::Validate(valid, queue);
    for (const auto index : {0u, 1u, 2u, 4u}) {
        auto packet = valid;
        packet[index] |= index == 0 ? 1u : index == 1 ? 0x10u : index == 2 ? 4u : 0x4000u;
        expectFailure([&] { AgcDriver::Pm4::Validate(packet, 0); });
    }
    auto graphics = valid;
    graphics[1] |= 0x40000000u;
    AgcDriver::Pm4::Validate(graphics, 0);
    expectFailure([&] { AgcDriver::Pm4::Validate(graphics, 0x20u); });
    expectFailure([&] { AgcDriver::Pm4::Validate(std::span(valid).first(4), 0); });
}

}

int main() {
    try {
        testReferencePackets();
        testPacketValidation();
        testCommand(writeCompute, sceAgcAcbPrimeUtcl2GetSize, true);
        testCommand(writeDraw, sceAgcDcbPrimeUtcl2GetSize, false);
        check(sceAgcAcbPrimeUtcl2GetSize() == sceAgcDcbPrimeUtcl2GetSize(), "compute and draw prime sizes differ");
        std::puts("AGC prime UTCL2 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
